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

  int vmait = pvma->vm_id;
  
  while (vmait != vmaid) 
  {
    if(pvma == NULL)
	  return NULL;
    pvma = pvma->vm_next; 
    vmait = pvma->vm_id;
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
  
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) 
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  /*Attempt to increase limit to get space */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); 
  int inc_sz = PAGING_PAGE_ALIGNSZ(size); 
  //int inc_limit_ret
  int old_sbrk ;
  old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
  if( inc_vma_limit(caller, vmaid, inc_sz) == 0 ) {
    /*Successful increase limit */
    caller->mm->symrgtbl[rgid].rg_start = old_sbrk; 
    caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size; 
    *alloc_addr = old_sbrk; //trả về 

    cur_vma->sbrk = old_sbrk + inc_sz;
    }
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table) 
  *@size: allocated size 
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  

  /* TODO: Manage the collect freed region to freerg_list */
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  

  int numpages = (PAGING_PAGE_ALIGNSZ(currg->rg_end - currg->rg_start)) / PAGING_PAGESZ;
  int pgn_start = PAGING_PGN(currg->rg_start);
  for(int i = pgn_start; i < (pgn_start + numpages); i++) {
    CLRBIT((caller->mm->pgd[i]), PAGING_PTE_PRESENT_MASK);
    
  }
  
  struct vm_rg_struct *free_rg = malloc(sizeof(struct vm_rg_struct));
  free_rg->rg_start = currg->rg_start;
  free_rg->rg_end = PAGING_PAGE_ALIGNSZ(currg->rg_end);
  /*enlist the obsoleted memory region*/
  enlist_vm_freerg_list(caller->mm, free_rg);

  
  currg->rg_end = -1;
  currg->rg_start = -1;
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
  //printf("Data in RAM\n");
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
            return 0; // Success
        }
        current_node = current_node->pg_next; // Move to the next node in the list
    }

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
  uint32_t pte = mm->pgd[pgn]; 
  if (!PAGING_PAGE_PRESENT(pte)) 
  {

    //printf("Page khong ton tai\n");
    fpn = NULL;
    return -1; // non exist in RAM or SWP, bị freed 
  }
  else { //chưa bị delete
    if(!PAGING_PAGE_IN_SWAP(pte)) { 
      *fpn = PAGING_FPN(pte);
      set_page_hit_cur_to_zero(mm, pgn);
      return 0;
    }
     else { 
      printf("Page trong SWAP\n");
      int tgtfpn = PAGING_SWP(pte);
      int vicfpn;
        int vicpgn, swpfpn; 
        uint32_t vicpte;

        /* TODO: Play with your paging theory here */
        uint32_t * ret_ptbl = NULL;
        find_victim_page(caller->mm, &vicpgn, &ret_ptbl);

        vicpte = ret_ptbl[vicpgn]; 
        if(!PAGING_PAGE_IN_SWAP(vicpte)) { 
          vicfpn = PAGING_FPN(vicpte); 
        }
        else{
          while(PAGING_PAGE_IN_SWAP(vicpte)){
            find_victim_page(caller->mm, &vicpgn, &ret_ptbl);
            vicpte = ret_ptbl[vicpgn];
          }
          vicfpn = PAGING_FPN(vicpte);
        }
      
        /* Get free frame in MEMSWP */
        MEMPHY_get_freefp(caller->active_mswp, &swpfpn);

        /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
        /* Copy victim frame to swap */
        MEMPHY_dump(caller->mram); 
        MEMPHY_dump(caller->active_mswp);
        __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
        MEMPHY_dump(caller->mram); 
        MEMPHY_dump(caller->active_mswp);
        int exist = 1; 
        if(!PAGING_PAGE_PRESENT(ret_ptbl[vicpgn])) {
          exist = 0;
        }
        pte_set_swap(ret_ptbl + vicpgn, 0, swpfpn); 
        if(!exist) CLRBIT(*(ret_ptbl + vicpgn), PAGING_PTE_PRESENT_MASK);
        
        *fpn = vicfpn;
        
        pte_set_fpn(mm->pgd + pgn, vicfpn); 
        MEMPHY_put_freefp(caller->active_mswp, tgtfpn);
        struct pgn_t* tmp = global_lru;
        while(tmp!=NULL){
          tmp->cur=tmp->cur+1;
          tmp=tmp->pg_next;
        }
        enlist_pgn_node(&global_lru, pgn, caller->mm); 
        caller->mm->lru_pgn = global_lru;
      
    }
  }

  MEMPHY_dump(caller->mram);
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


  pg_getval(caller->mm, currg->rg_start + offset, data, caller); 

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

  pg_setval(caller->mm, currg->rg_start + offset, value, caller); 

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
  print_pgtbl(proc, 0, -1);
#endif
  MEMPHY_dump(proc->mram);
#endif
  int ret;
  ret = __write(proc, 0, destination, offset, data);
  //printf("write finish\n");
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
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz) 
{
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz); 
  int incnumpage =  inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt); 
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  int old_end = cur_vma->vm_end; 
  
  
  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0) 
    return -1; /*Overlap and failed allocation */

  /* The obtained vm area (only) 
   * now will be alloc real ram region */
  cur_vma->vm_end += inc_sz; 
  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
                    old_end, incnumpage , newrg) < 0)
    return -1; 
  return 0;

}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn, uint32_t** ret_ptbl) 
{
  struct pgn_t *pg = global_lru; 

  /* TODO: Implement the theorical mechanism to find the victim page */
  if(pg == NULL) {
    *retpgn = -1;
    *ret_ptbl = NULL;
    return -1;
  }
  else if(pg->pg_next == NULL) {
    *retpgn = pg->pgn;
    global_lru = NULL;
  }
  else {
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

  (*ret_ptbl) = pg->owner->pgd; 
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
  int align_size = PAGING_PAGE_ALIGNSZ(size); 

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  int i = 0;
  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    i++;
    if ((rgit->rg_start % PAGING_PAGESZ != 0) || (rgit->rg_end % PAGING_PAGESZ != 0)) {
    }

    if (rgit->rg_start + align_size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;
      //Set bit present
      int numpages = (PAGING_PAGE_ALIGNSZ(newrg->rg_end - newrg->rg_start)) / PAGING_PAGESZ;
      int pgn_start = PAGING_PGN(newrg->rg_start);


      /* Update left space in chosen region */
      if (rgit->rg_start + align_size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + align_size;
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
          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;
          rgit->rg_next = NULL;
        }
      }
      break; 
    }
    else
    {
      rgit = rgit->rg_next;	
    }
  }

 if(newrg->rg_start == -1) 
   return -1;

 return 0;
}

//#endif

