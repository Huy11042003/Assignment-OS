//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct* rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma= mm->mmap;

  if(mm->mmap == NULL)
    return NULL;

  int vmait = pvma->vm_id;//0;
  
  while (vmait != vmaid) //đổi < thành !=
  {
    if(pvma == NULL)
	  return NULL;
    // Them vao gi do? vmait = pvma->vm_id; || vmait++ 
    pvma = pvma->vm_next; 
    vmait = pvma->vm_id; //thêm
  }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;
//regionn bị free thì ntn?
  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbols table)
 *@size: allocated size 
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  //pthread_mutex_lock(&mem_lock); //lock
  printf("Alloc here! proc %d, vmaid %d, rgid %d, size %d\n", caller->pid, vmaid, rgid, size);
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;
  //có cần align không? -> align cái start là ok
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) // Có free đủ lớn, cái này được map sẵn rồi
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    printf("Alloc get enough free region, start %ld, end %ld\n", rgnode.rg_start, rgnode.rg_end);

    *alloc_addr = rgnode.rg_start;
    //pthread_mutex_unlock(&mem_lock); //unlock
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  /*Attempt to increase limit to get space */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); //Có lẽ chỉ làm việc với 1 vma thôi
  int inc_sz = PAGING_PAGE_ALIGNSZ(size); //thầy làm r nè 
  //int inc_limit_ret
  int old_sbrk ;
  old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
  if( inc_vma_limit(caller, vmaid, inc_sz) == 0 ) {
    /*Successful increase limit */
    caller->mm->symrgtbl[rgid].rg_start = old_sbrk; 
    caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size; //end của cái này không cần align, oke rồi
    *alloc_addr = old_sbrk; //trả về 

    cur_vma->sbrk = old_sbrk + inc_sz;// Cập nhật sbrk?? aligned
    //Cập nhật pgd? hàm inc trên làm r
    printf("Alloc need to incr, start %ld, end %ld, sbrk %ld\n", caller->mm->symrgtbl[rgid].rg_start, caller->mm->symrgtbl[rgid].rg_end, cur_vma->sbrk);
  }
  //pthread_mutex_unlock(&mem_lock);  //unlock
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table) -> biến này bị free -> set region nó =-1? Ok
                                                                       còn chỗ cho symb khác k? tùy thuộc vào alloc lại ntn
 *@size: allocated size 
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  //pthread_mutex_lock(&mem_lock);
  printf("Free here, proc %d, vmaid %d, rgid %d\n", caller->pid, vmaid, rgid);
  //ncl set cái rgid đó start = end = -1,
  //Set bit present = 0
  //Có trong RAM/ SWP thì xóa (cho vào freelist)
  //Làm gì với đống BYTE? -> Muốn vào được BYTE phải thông qua virtual, RAM/SWP này 
  //trả vào free list
  // struct vm_rg_struct rgnode; -> chẳng biết làm gì
  
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) {
    //pthread_mutex_unlock(&mem_lock);
    return -1;
  }

  /* TODO: Manage the collect freed region to freerg_list */
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  //Xóa trong RAM/ SWP, đem ra ngoài RAM hết!
  //Cần có được pgn, pte, start/end -> byte addressable, chưa align đâu
  if(currg->rg_start % PAGING_PAGESZ != 0) { //Kiểm tra lại page_align
    printf("Region start is not aligned, currg->rg_start %ld\n", currg->rg_start);
  }
  if(currg->rg_start == -1) { //Double free, unsigned ??? <0 k dc dau
    printf("Double free region %d, free_rg->rg_start %ld, free_rg->rg_end %ld\n", rgid, currg->rg_start, currg->rg_end);
    //pthread_mutex_unlock(&mem_lock);
    return 0;
  }
  int numpages = (PAGING_PAGE_ALIGNSZ(currg->rg_end - currg->rg_start)) / PAGING_PAGESZ;
  int pgn_start = PAGING_PGN(currg->rg_start);
  // Set lại PTE, set present = 0. Set byte về NULL?
  for(int i = pgn_start; i < (pgn_start + numpages); i++) {
    CLRBIT((caller->mm->pgd[i]), PAGING_PTE_PRESENT_MASK);
    /* Cần set các byte về NULL không?
    if(!PAGING_PAGE_IN_SWAP(caller->mm->pgd[i])) { //trong RAM 
    }*/
  }
  
  //Tạo 1 free region. Thì nó vẫn có fpn cũ mà
  struct vm_rg_struct *free_rg = malloc(sizeof(struct vm_rg_struct));
  free_rg->rg_start = currg->rg_start;
  free_rg->rg_end = PAGING_PAGE_ALIGNSZ(currg->rg_end);
  /*enlist the obsoleted memory region*/
  enlist_vm_freerg_list(caller->mm, free_rg);

  printf("Free duoc 1 region, free_rg->rg_start %ld, free_rg->rg_end %ld\n", free_rg->rg_start, free_rg->rg_end);
  // Set cho cái variable đó không còn sài (write/read) được nữa
  printf("Free region list: \n");
  print_list_rg(caller->mm->mmap->vm_freerg_list);
  currg->rg_end = -1;
  currg->rg_start = -1;
  //pthread_mutex_unlock(&mem_lock);
  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr, ret;
  pthread_mutex_lock(&mem_lock);
  /* By default using vmaid = 0 */
  ret = __alloc(proc, 0, reg_index, size, &addr);
  printf("Data in RAM\n");
  MEMPHY_dump(proc->mram);
  pthread_mutex_unlock(&mem_lock);
  return ret;
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
  int ret;
  pthread_mutex_lock(&mem_lock);
  ret = __free(proc, 0, reg_index);
  pthread_mutex_unlock(&mem_lock);
  return ret;
}


int set_page_hit_cur_to_zero(struct mm_struct *mm, int pgn) {
    struct pgn_t *current_node = global_lru; // Assume global_lru is the head of your linked list

    while (current_node != NULL) {
        if (current_node->pgn == pgn && current_node->owner == mm) {
            // Found the page node, set cur to zero
            current_node->cur = 0;
            printf("Page hit: Page number %d's 'cur' value set to 0\n", pgn);
            return 0; // Success
        }
        current_node = current_node->pg_next; // Move to the next node in the list
    }

    printf("Page number %d not found in the list\n", pgn);
    return -1; // Indicate failure: page not found
}


/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller) 
{
  uint32_t pte = mm->pgd[pgn]; // mm sài chung???-> riêng! nma rồi vào trong pgd là riêng 
  printf("Tim page de read/write\n");
  if (!PAGING_PAGE_PRESENT(pte)) //free roi thi tinh sao? free->bit present/ swap-> bit swap
  {

    printf("Page khong ton tai\n");
    fpn = NULL;
    return -1; // non exist in RAM or SWP, bị freed 
  }
  else { //chưa bị delete
    if(!PAGING_PAGE_IN_SWAP(pte)) { //không trong swp -> khỏe rồi, trong RAM
      *fpn = PAGING_FPN(pte);
      printf("Page trong RAM\n");
      set_page_hit_cur_to_zero(mm, pgn);
      return 0;
    }
    // phải extract bit chứ làm như sau sai đó -> if(PAGING_PAGE_IN_SWAP(pte)) /* Page is not online, it's in SWAP, make it actively living */
    else { //không bằng 0 thì không phải = 1 đâu, nó lớn lắm 1xxxxx...
      printf("Page trong SWAP\n");
      int tgtfpn = PAGING_SWP(pte);//the target frame storing our variable, nằm trong SWAP
      //Trước tiên cứ tìm freeframe đã, nmà theo yêu cầu của thầy là không cần 
      int vicfpn;
      /*if(MEMPHY_get_freefp(caller->mram, &vicfpn) == 0) {
        __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);
        *fpn = vicfpn;
        pte_set_fpn(mm->pgd + pgn, vicfpn); // cập nhật lại fpn của pte(hay mm->pgd[pgn]) trỏ tới 
        enlist_pgn_node(&global_fifo, pgn, caller->mm); //Dùng global
        caller->mm->fifo_pgn = global_fifo;

      } else {  //không có frame trống
      */
        printf("Tim victim\n");
        int vicpgn, swpfpn; 
        uint32_t vicpte;

        /* TODO: Play with your paging theory here */
        /* Find victim page */ //victim page có thể thuộc proc khác, tức pgd khác
        uint32_t * ret_ptbl = NULL;
        find_victim_page(caller->mm, &vicpgn, &ret_ptbl);

        // làm:... convert victim page -> victim frame
        vicpte = ret_ptbl[vicpgn]; //caller->mm->pgd[vicpgn]; // Chac gi no thuoc pgd cua mm cua caller?
        if(!PAGING_PAGE_IN_SWAP(vicpte)) { 
          printf("Victim o trong RAM\n");
          vicfpn = PAGING_FPN(vicpte); //Lấy được cái fpn (đang trong RAM) của proc nào đó (không phải thằng caller) 
                                      //Thằng này sau khi swap out ra thì sẽ được tgtfpn copy content vô
        }
        else{
          while(PAGING_PAGE_IN_SWAP(vicpte)){
            printf("Victim o trong SWAP\n"); //lỗi này căng
            find_victim_page(caller->mm, &vicpgn, &ret_ptbl);
            vicpte = ret_ptbl[vicpgn];
          }
          vicfpn = PAGING_FPN(vicpte);
        }
      
        /* Get free frame in MEMSWP */
        MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

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

        printf("target frame in SWAP %d\n", tgtfpn);
        printf("victim page %d was mapped to victim frame %d\n", vicpgn, vicfpn);
        printf("new frame number in SWAP to store victim %d\n", swpfpn);

        // Cập nhật lại PTE chứ, của thằng owner (ret_ptbl) bị swap out ấy
        //PAGING_PTE_SET_IN_SWAP(ret_ptbl[vicpgn]);
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
        printf("DATA in SWAP: ");
        MEMPHY_dump(caller->active_mswp);
        printf("...xong roi\n");
        /* Copy target frame from swap to mem */
        printf("data in RAM before copy content\n");
        MEMPHY_dump(caller->mram); 
        printf("...xong roi\n");
        printf("data in SWAP before copy content\n");
        MEMPHY_dump(caller->active_mswp);
        printf("...xong roi\n");
        __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn); //chắc gì nó cùng proccess?
        printf("data in RAM after copy content\n");
        MEMPHY_dump(caller->mram); 
        printf("...xong roi\n");
        printf("data in SWAP after copy content\n");
        MEMPHY_dump(caller->active_mswp);
        printf("...xong roi\n");
        *fpn = vicfpn;
        //*fpn = ret_fr; //đây nè      //int ret_fr = 0; //để đảm bảo cập nhật cho fpn chính xác hơn, do fpn là ptr

        /* Update page table */
        //pte_set_swap() &mm->pgd;

        /* Update its online status of the target page */
        //pte_set_fpn() & mm->pgd[pgn];
        pte_set_fpn(mm->pgd + pgn, vicfpn); // cập nhật lại fpn của !!!pte(hay mm->pgd[pgn]) trỏ tới 
        // Cho thằng SWAP thêm free frame
        MEMPHY_put_freefp(caller->active_mswp, tgtfpn);

        //Vô Ram rồi, nên là phải vào fifo, thằng pgn ở trong Virtual
        //enlist_pgn_node(&caller->mm->fifo_pgn,pgn); lấy pgn là đúng rồi do trong virtual mà
        struct pgn_t* tmp = global_lru;
        while(tmp!=NULL){
          tmp->cur=tmp->cur+1;
          tmp=tmp->pg_next;
        }
        enlist_pgn_node(&global_lru, pgn, caller->mm); //Dùng global
        //chỉ mới enlist thôi, có biết cái mới thuộc proc nào không?
        caller->mm->lru_pgn = global_lru;
      
    }
  }

  //*fpn = PAGING_FPN(pte);
  printf("DATA in RAM ");
  MEMPHY_dump(caller->mram);
  printf("...xong roi\n");
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to access 
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  printf("fpn %d, read at %d in physic\n",fpn, phyaddr);
  MEMPHY_read(caller->mram,phyaddr, data);

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;


  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  printf("fpn %d, write at %d in physic\n",fpn, phyaddr);

  MEMPHY_write(caller->mram,phyaddr, value);

   return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;
  printf("Read here! Proc %d, vmaid %d, rgid %d, rg->start %ld (256?), rg->end %ld\n", caller->pid, vmaid, rgid, currg->rg_start, currg->rg_end);
//Thêm để kiểm tra xem region muốn vào bị free chưa
  if( (currg->rg_start == currg->rg_end) && (currg->rg_start == -1) ) {
    printf("Read in freed region\n");
    //return -1;
  }
  //Thêm để kiểm tra nó có đọc ngoài vùng không?
  if( (currg->rg_start + offset) >= currg->rg_end ) {
    printf("Invalid access for this region, Out of range\n");
    //return -1;
  }

  pg_getval(caller->mm, currg->rg_start + offset, data, caller); //Ủa chứ area làm gì?

  return 0;
}


/*pgwrite - PAGING-based read a region memory */
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) 
{
  BYTE data;
  pthread_mutex_lock(&mem_lock);
  int val = __read(proc, 0, source, offset, &data);
  destination = (uint32_t) data;
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
  pthread_mutex_unlock(&mem_lock);
  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;
  printf("Write here! Proc %d, vmaid %d, rgid %d, rg->start %ld (256?), rg->end %ld\n", caller->pid, vmaid, rgid, currg->rg_start, currg->rg_end);

  //Thêm để kiểm tra xem region muốn vào bị free chưa
  if( (currg->rg_start == currg->rg_end) && (currg->rg_start == -1) ) {
    printf("Write in freed region\n");
    //return -1;
  }
  //Thêm để kiểm tra nó có đọc ngoài vùng không?
  if( (currg->rg_start + offset) >= currg->rg_end ) {
    printf("Invalid access for this region, Out of range\n");
    //return -1;
  }
  printf("write at %ld in virtual\n", currg->rg_start + offset);
  pg_setval(caller->mm, currg->rg_start + offset, value, caller); //hàm này vẫn phải ktra cho chắc

  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset)
{
  pthread_mutex_lock(&mem_lock);
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
  int ret;
  ret = __write(proc, 0, destination, offset, data);
  printf("write finish\n");
  MEMPHY_dump(proc->mram);
  pthread_mutex_unlock(&mem_lock);
  return ret;
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;

  pthread_mutex_lock(&mem_lock);
  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];
    //chỉnh lại tí
    /*if (!PAGING_PAGE_PRESENT(pte)) { // page không tồn tại, khi bị free -> người ta tới dọn thôi mà@
      continue;
    } else {*/
      if(!PAGING_PAGE_IN_SWAP(pte)) {
        fpn = PAGING_FPN(pte);
        MEMPHY_put_freefp(caller->mram, fpn);
      } else {
        fpn = PAGING_SWP(pte);
        MEMPHY_put_freefp(caller->active_mswp, fpn);    
      }
  }
  pthread_mutex_unlock(&mem_lock);
  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{ //ủa mà size với alignedsz như nhau mà???
  struct vm_rg_struct * newrg;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma start
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct *vma = caller->mm->mmap;

  /* TODO validate the planned memory area is not overlapped */
  if((vmastart < vma->sbrk) || (vmaend < vma->sbrk) || (vmaend < vmastart)) return -1;

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size 
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz) // chỉ gọi khi alloc thiếu
{
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz); //trc khi vào đây inc_sz đã align rồi, = inc_sz
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt); //cái inc_sz đã align r đó nghen
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  int old_end = cur_vma->vm_end; // = sbrk?
  printf("to incr vma limit, vmaid %d, inc_amt %d ?=? inc_sz %d\n", vmaid, inc_amt, inc_sz);
  
  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0) //mấy thằng chưa align ấy
    return -1; /*Overlap and failed allocation */

  /* The obtained vm area (only) 
   * now will be alloc real ram region */
  printf("Check truoc khi thay doi cur_vma->vm_start %ld, cur_vma->vm_end %ld\n", cur_vma->vm_start, cur_vma->vm_end);
  cur_vma->vm_end += inc_sz; //cập nhật luôn cái end cơ à, không phải end cố định còn sbrk tăng giảm à :))
                        //ủa sao không cộng cái align? cái inc_sz cx align rồi
  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
                    old_end, incnumpage , newrg) < 0)
    return -1; /* Map the memory to MEMRAM */
  printf("Check after, area, area_sbrk ?==? newrg:\n");
  printf("cur_vma->vm_start %ld, cur_vma->vm_end %ld\n", cur_vma->vm_start, cur_vma->vm_end);
  printf("area_sbrk->rg_start %ld, area_sbrk->rg_end %ld\n", area->rg_start, area->rg_end);
  printf("newrg->start %ld, newrg->end %ld\n", newrg->rg_start, newrg->rg_end);
        
  //thêm, có cái newrg xong làm gì? newrg với area là 1 à?
  return 0;

}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn, uint32_t** ret_ptbl) 
{
  struct pgn_t *pg = global_lru; //thay luôn mm->fifo_pgn;
  printf("Before find victim\n");
  print_list_pgn(global_lru);

  /* TODO: Implement the theorical mechanism to find the victim page */
  if(pg == NULL) {
    *retpgn = -1;
    *ret_ptbl = NULL;
    return -1;
  }
  else if(pg->pg_next == NULL) { //Chỉ có 1 Node
    *retpgn = pg->pgn;
    global_lru = NULL;
  }
  else { //2 Node trở lên     //dung lru
    int max=0;
    struct pgn_t *temp = global_lru;
    while(temp != NULL) { 
      if( temp->cur > max){
        max=temp->cur;
      }
      temp = temp->pg_next; 
    }
    
    struct pgn_t *tmp;
    while(pg!= NULL) { 
      if(pg->cur == max){
        if(pg->pg_next == NULL)
          break;
        else{
          tmp = pg->pg_next;
          break;
        }
      }
      pg = pg->pg_next; 
    }

    struct pgn_t *t = global_lru;
    while (t->pg_next!=pg){
      t=t->pg_next;
    }
    if (pg->pg_next == NULL)
    {
      t->pg_next=NULL;
    }
    else
    {
      t->pg_next=tmp;
    }
    *retpgn = pg->pgn; 
  }

  //tìm ra cái victim page chứ không biết nó thuộc proc nào
  //Cập nhật PTE
  (*ret_ptbl) = pg->owner->pgd; // tới đúng cái proc chứa nó, chứ k phải mm, 
                                //mm là của thằng đang cần, thằng bị swap có thể khác
  if((*ret_ptbl) == NULL) printf("ret_ptbl == NULL\n");
  else printf("ret_ptbl != NULL, victim page %d\n", (*retpgn));
  free(pg);
  mm->lru_pgn = global_lru;
  printf("After find victim\n");
  print_list_pgn(global_lru);
  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size 
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  //thêm -> cần align nên size cần chỉnh lại tí
  int align_size = PAGING_PAGE_ALIGNSZ(size); //size = 1 -> cấp luôn 256 byte, = 257 thì 2*256 byte (2 pages)

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  print_list_rg(rgit);
  if (rgit == NULL)
    return -1;

  printf("Get free vmrg in list region, size %d align_size: %d\n", size, align_size);
  int i = 0;
  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    i++;
    printf("Duyet thang thu %d rg_start %ld, rg_end %ld\n", i, rgit->rg_start, rgit->rg_end);
    if ((rgit->rg_start % PAGING_PAGESZ != 0) || (rgit->rg_end % PAGING_PAGESZ != 0)) {//thêm để kiểm tra xem nó align chưa
      printf("Not aligned???\n");
    }

    if (rgit->rg_start + align_size <= rgit->rg_end) // (rgit->rg_start + size <= rgit->rg_end) -> kiểm tra tới tận align_size
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size; //end thì tới size thôi
      printf("Chon thang nay, new_rg_start %ld, new_rg_end %ld\n",newrg->rg_start, newrg->rg_end);
      //Set bit present
      int numpages = (PAGING_PAGE_ALIGNSZ(newrg->rg_end - newrg->rg_start)) / PAGING_PAGESZ;
      int pgn_start = PAGING_PGN(newrg->rg_start);
      // Set lại PTE, set present = 1. 
      for(int i = pgn_start; i < (pgn_start + numpages); i++) {
        SETBIT((caller->mm->pgd[i]), PAGING_PTE_PRESENT_MASK);
        if(!PAGING_PAGE_IN_SWAP(caller->mm->pgd[i])) {
          int fpn = PAGING_FPN(caller->mm->pgd[i]);
          printf("proc %d, page %d is mapped to frame %d in RAM\n", caller->pid, i, fpn);
        }
        else {
          int fpn = PAGING_SWP(caller->mm->pgd[i]);
          printf("proc %d, page %d is mapped to frame %d in SWAP\n", caller->pid, i, fpn);
        }
      }
      /*for(int i = pgn_start + numpages; i < (pgn_start + numpages + 3); i++) {
        if(!PAGING_PAGE_IN_SWAP(caller->mm->pgd[i])) {
          int fpn = PAGING_FPN(caller->mm->pgd[i]);
          printf("proc %d, page %d is mapped to frame %d in RAM\n", caller->pid, i, fpn);
        }
        else {
          int fpn = PAGING_SWP(caller->mm->pgd[i]);
          printf("proc %d, page %d is mapped to frame %d in SWAP\n", caller->pid, i, fpn);
        }
      }*/

      /* Update left space in chosen region */
      printf("Cap nhat remaining free region\n");
      if (rgit->rg_start + align_size < rgit->rg_end)//(rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + align_size;//rgit->rg_start = rgit->rg_start + size; -> start cần align
        printf("free region con du cho, rg_start sau upd %ld\n", rgit->rg_start);
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;
          printf("Phai qua free region next ?256?, rg_start %ld rg_end %ld\n", rgit->rg_start, rgit->rg_end);
          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;	//dummy, size 0 region
          printf("Het cho roi, rg_start %ld rg_end %ld\n", rgit->rg_start, rgit->rg_end);
          rgit->rg_next = NULL;
        }
      }
      break; // thầy quên
    }
    else
    {
      rgit = rgit->rg_next;	// Traverse next rg
    }
  }

 if(newrg->rg_start == -1) // new region not found
   return -1;

 return 0;
}

//#endif

