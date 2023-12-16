//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

struct pgn_t* global_lru = NULL;

/* 
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type -> 0
             int swpoff) //swap offset
{
  if (pre != 0) { //Tồn tại, chưa bị free
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0) 
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 
    } else { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT); 
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;   
}

/* 
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

void set_bit(uint32_t *var, int bit_index, int value) {
    uint32_t mask = 1u << bit_index;
    if (value) {
        *var |= mask;    // set bit to 1
    } else {
        *var &= ~mask;   // set bit to 0
    }
}

/* 
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    //int j = 12;
  /*for(int i = 12; i >= 0; i--) {
    int bit = (fpn >> i) & 1;
    set_bit(pte, i, bit);
  }*/
  //PAGING_PTE_FPN_LOBIT  PAGING_ADDR_OFFST_LOBIT
  ///PAGING_ADDR_FPN_LOBIT  PAGING_ADDR_PGN_LOBIT
  int fpn_t = PAGING_FPN(*pte); //PAGING_PGN(*pte) -> disaster!!!!!!!
  printf("frame %d is mapped to frame %d in RAM\n", fpn, fpn_t);

  return 0;
}


/* 
 * vmap_page_range - map a range of page at aligned address
 */
int vmap_page_range(struct pcb_t *caller, // process call
                                int addr, // start address which is aligned to pagesz
                               int pgnum, // num of mapping page
           struct framephy_struct *frames,// list of the mapped frames
              struct vm_rg_struct *ret_rg)// return mapped region, the real mapped fp
{                                         // no guarantee all given pages are mapped
  //uint32_t * pte = malloc(sizeof(uint32_t));
  //struct framephy_struct *fpit = malloc(sizeof(struct framephy_struct));
  //int  fpn;
  int pgit = 0;
  int pgn = PAGING_PGN(addr); // chuyen tu virtual -> index cua PTE

  ret_rg->rg_end = ret_rg->rg_start = addr; // at least the very first space is usable

  //fpit->fp_next = frames; //free frame cbi map

  /* TODO map range of frame to address space (virtual address)
   *      [addr (start) to addr(end -> start + ...) + pgnum*PAGING_PAGESZ (So address dc tang len)
   *      in page table caller->mm->pgd[]
   */
  // tom lai la chuyen doi PTE
  // Lưu ý là có trường hợp map không đủ, vì có thể địa chỉ nó cấp nhiều quá, tràn luôn khỏi số page có thể cấp của pgd
  struct framephy_struct *fpit2 = frames;
  for( ; pgit < pgnum; pgit++) {
    if(fpit2 == NULL) {
      printf("Not enough frame in alloc\n");
      return -1; // k đủ frame
    }
    if((pgn+pgit) >= PAGING_MAX_PGN) {
      printf("Out_of_range pgd of alloc\n");
      return -1; //hết chỗ rồi
    }
    pte_set_fpn(caller->mm->pgd + (pgn+pgit), fpit2->fpn); //map cái pte tại idx với cái frame trống 
    //kiểm tra khi alloc
    printf("proc %d, page %d is mapped to frame %d\n", caller->pid, pgn+pgit, fpit2->fpn);
    int fpn_t = PAGING_FPN(caller->mm->pgd[pgn+pgit]);
    printf("proc %d, page %d is mapped to frame %d in RAM\n", caller->pid, pgn+pgit, fpn_t);

   /* Tracking for later page replacement activities (if needed)
    * Enqueue new usage page */

    struct pgn_t* tmp = global_lru;
    while(tmp!=NULL)
    {
      tmp->cur=tmp->cur+1;
      tmp=tmp->pg_next;
    }
   
    enlist_pgn_node(&global_lru, pgn+pgit, caller->mm); //Dùng global
    //enlist_pgn_node(&caller->mm->fifo_pgn, pgn+pgit); // cho thì dùng, FIFO
    caller->mm->lru_pgn = global_lru;
    ret_rg->rg_end = ret_rg->rg_end + PAGING_PAGESZ;
    fpit2 = fpit2->fp_next; //qua frame tiếp theo
  }

  // Dùng xong free chứ?
  /*free(fpit);
  while(frames != NULL) {
    fpit = frames;alloc_pages_range
    frames = frames->fp_next;
    free(fpit);
  }*/
  return 0;
}

/* 
 * alloc_pages_range - allocate req_pgnum of frame in ram, tìm cho đủ frame trống, k đủ thì làm cho đủ
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct** frm_lst)
{
  int pgit, fpn;
  //struct framephy_struct *newfp_str;
  printf("Kiem cho du chung nay free_frame trong RAM %d\n", req_pgnum);
  for(pgit = 0; pgit < req_pgnum; pgit++)
  {
    if(MEMPHY_get_freefp(caller->mram, &fpn) == 0) // alloc thi cbi dc dung -> cut khoi freelist
   {
     // TODO thay quen :))
      printf("Co free frame\n");
      struct framephy_struct* tobe_add = malloc(sizeof(struct framephy_struct));
      tobe_add->fpn = fpn;
      tobe_add->fp_next = (*frm_lst);
      (*frm_lst) = tobe_add;

   } else {  // ERROR CODE of obtaining some but not enough frames
      // tìm victim, sau đó bỏ vào freelist frame, giảm pgit, chạy lại
      printf("Tim victim\n");
      int vicpgn, swpfpn; 
      int vicfpn;
      uint32_t vicpte;

      /* Find victim page */ //victim page có thể thuộc proc khác, tức pgd khác
      uint32_t * ret_ptbl = NULL;
      find_victim_page(caller->mm, &vicpgn, &ret_ptbl);
      if(vicpgn == -1) {
        printf("Cannot find victim page! %d\n", vicpgn);
      }

      // làm:... convert victim page -> victim frame
      if(ret_ptbl + vicpgn != NULL) {
        printf("ret_ptbl != NULL, victim page %d\n", vicpgn);
      }
      else {
        printf("ret_ptbl == NULL, %p, victim page %d\n", ret_ptbl, vicpgn);
      }
      vicpte = ret_ptbl[vicpgn]; //caller->mm->pgd[vicpgn]; // Chac gi no thuoc pgd cua mm cua caller?
      //Chắc gì đã trong frame?
      
      if(!PAGING_PAGE_IN_SWAP(vicpte)) { 
        vicfpn = PAGING_FPN(vicpte); //Lấy được cái fpn (đang trong RAM) của proc nào đó (không phải thằng caller) 
                                    //Thằng này sau khi swap out ra thì sẽ được tgtfpn copy content vô
        printf("Victim o trong RAM, victim frame %d\n", vicfpn);
      }
      else{
        while(PAGING_PAGE_IN_SWAP(vicpte)){
          printf("Victim o trong SWAP!!!\n"); //lỗi này căng
          find_victim_page(caller->mm, &vicpgn, &ret_ptbl);
          vicpte = ret_ptbl[vicpgn];
        }
        vicfpn = PAGING_FPN(vicpte);
      }
     
      /* Get free frame in MEMSWP */
      MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
      printf("this frame then move to frame %d in SWAP\n", swpfpn);
      /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
      /* Copy victim frame to swap */
      printf("data in RAM before copy content\n");
      MEMPHY_dump(caller->mram); 
      printf("...xong roi\n");
      printf("data in SWAP before copy content\n");
      MEMPHY_dump(caller->active_mswp);
      printf("...xong roi\n");
      __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
      printf("data in RAM after copy content\n");
      MEMPHY_dump(caller->mram); 
      printf("...xong roi\n");
      printf("data in SWAP after copy content\n");
      MEMPHY_dump(caller->active_mswp);
      printf("...xong roi\n");
      printf("victim frame in RAM move to SWAP frame %d\n", swpfpn);
      // Cập nhật lại PTE chứ, của thằng owner (ret_ptbl) bị swap out ấy
      //Nếu trước đó nó đã bị free -> bit present = 0 thì vẫn giữ như vậy
      int exist = 1; 
      if(!PAGING_PAGE_PRESENT(ret_ptbl[vicpgn])) {
        printf("Frame nay bi free truoc do\n");
        exist = 0;
      }
      pte_set_swap(ret_ptbl + vicpgn, 0, swpfpn); // victim page của proc nào đó giờ PTE nó cập nhật về swpfpn
      if(!exist) CLRBIT(*(ret_ptbl + vicpgn), PAGING_PTE_PRESENT_MASK);
      if(!PAGING_PAGE_PRESENT(ret_ptbl[vicpgn])) {
        printf("Confirm giu lai\n");
      }

      //Thêm frame đó vào freeframe của RAM
      MEMPHY_put_freefp(caller->mram, vicfpn);
      
      //Chạy lại
      pgit--;
   } 
 }

  return 0;
}


/* 
 * vm_map_ram - do the mapping all vm area to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{ //NCL cái này chỉ gọi khi incr size thôi, map vô PTE đấy, làm cho nó "present/exist"
  struct framephy_struct *frm_lst = NULL;
  int ret_alloc;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide 
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst); //tìm cho đủ rồi bỏ vào frm_lst

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000) 
  {
#ifdef MMDBG
     printf("OOM: vm_map_ram out of memory \n");
#endif
     return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg); //map nhe, cap nhat cai free lsit moi xong

  if( (ret_rg->rg_start != astart) || (ret_rg->rg_end != aend) ) {
    printf("Mapped region is wrong?\n%ld %d %ld %d\n", ret_rg->rg_start, astart, ret_rg->rg_end, aend);
  }
  return 0;
}

/* Swap copy content page from source frame to destination frame 
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                struct memphy_struct *mpdst, int dstfpn) 
{
  int cellidx;
  int addrsrc,addrdst;
  for(cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct * vma = malloc(sizeof(struct vm_area_struct));

  mm->pgd = malloc(PAGING_MAX_PGN*sizeof(uint32_t));

  /* By default the owner comes with at least one vma */
  vma->vm_id = 0; //thầy ghi 1 nè, chuyển thành 0
  vma->vm_start = 0;
  vma->vm_end = vma->vm_start;
  vma->sbrk = vma->vm_start; //cái end với sbrk nói vậy cho vui :0
  struct vm_rg_struct *first_rg = init_vm_rg(vma->vm_start, vma->vm_end);
  enlist_vm_rg_node(&vma->vm_freerg_list, first_rg);

  vma->vm_next = NULL;
  vma->vm_mm = mm; /*point back to vma owner */

  mm->mmap = vma;
  //Thêm vô
  //Liệu caller->mm có khác mm?
  if(caller->mm != mm) {
    printf("caller->mm != mm\n");
  }
  mm->lru_pgn = global_lru;
  return 0;
}

struct vm_rg_struct* init_vm_rg(int rg_start, int rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct* rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, int pgn, struct mm_struct* owner)
{
  struct pgn_t* pnode = malloc(sizeof(struct pgn_t));
  pnode->cur = 1;
  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  pnode->owner = owner; //thêm
  *plist = pnode;
  
  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
   struct framephy_struct *fp = ifp;
 
   printf("print_list_fp: ");
   if (fp == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (fp != NULL )
   {
       printf("fp[%d]\n",fp->fpn);
       fp = fp->fp_next;
   }
   printf("\n");
   return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
   struct vm_rg_struct *rg = irg;
 
   printf("print_list_rg: ");
   if (rg == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (rg != NULL)
   {
       printf("rg[%ld->%ld]\n",rg->rg_start, rg->rg_end);
       rg = rg->rg_next;
   }
   printf("\n");
   return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
   struct vm_area_struct *vma = ivma;
 
   printf("print_list_vma: ");
   if (vma == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (vma != NULL )
   {
       printf("va[%ld->%ld]\n",vma->vm_start, vma->vm_end);
       vma = vma->vm_next;
   }
   printf("\n");
   return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
   printf("print_list_pgn: ");
   if (ip == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (ip != NULL )
   {
       printf("va[%d]-\n",ip->pgn);
       ip = ip->pg_next;
   }
   printf("n");
   return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start,pgn_end;
  int pgit;

  if(end == -1){
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl: %d - %d", start, end);
  if (caller == NULL) {printf("NULL caller\n"); return -1;}
    printf("\n");


  for(pgit = pgn_start; pgit < pgn_end; pgit++)
  {
     printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  return 0;
}

//#endif

