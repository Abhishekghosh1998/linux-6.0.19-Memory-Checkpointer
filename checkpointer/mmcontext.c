#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/cpu.h>

#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/sched/mm.h>
#include <linux/sched/coredump.h>
#include <linux/sched/numa_balancing.h>
#include <linux/sched/task.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/memremap.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/export.h>
#include <linux/delayacct.h>
#include <linux/init.h>
#include <linux/pfn_t.h>
#include <linux/writeback.h>
#include <linux/memcontrol.h>
#include <linux/mmu_notifier.h>
#include <linux/swapops.h>
#include <linux/elf.h>
#include <linux/gfp.h>
#include <linux/migrate.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/userfaultfd_k.h>
#include <linux/dax.h>
#include <linux/oom.h>
#include <linux/numa.h>
#include <linux/perf_event.h>
#include <linux/ptrace.h>
#include <linux/vmalloc.h>

#include <trace/events/kmem.h>

#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/pgalloc.h>
#include <linux/uaccess.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>

// #include "pgalloc-track.h"
// #include "internal.h"
// #include "swap.h"

static inline int
walk_present_pte(/* struct vm_area_struct *dst_vma, */ struct vm_area_struct *src_vma,
		 /* pte_t *dst_pte, */ pte_t *src_pte, unsigned long addr/*,  int *rss, */
		 /* struct page **prealloc */
		 /* THe following field is just for testing purpose*/, pmd_t *src_pmd)
{
	struct mm_struct *src_mm = src_vma->vm_mm;
	unsigned long vm_flags = src_vma->vm_flags;
	pte_t pte = *src_pte;
	//struct page *page;
										/* trace_printk("\n"); */

	// page = vm_normal_page(src_vma, addr, pte);
	// if (page && PageAnon(page)) {
	// 	/*
	// 	 * If this page may have been pinned by the parent process,
	// 	 * copy the page immediately for the child so that we'll always
	// 	 * guarantee the pinned page won't be randomly replaced in the
	// 	 * future.
	// 	 */
	// 	get_page(page);
	// 	if (unlikely(page_try_dup_anon_rmap(page, false, src_vma))) {
	// 		/* Page maybe pinned, we have to copy. */
	// 		put_page(page);
	// 		return copy_present_page(dst_vma, src_vma, dst_pte, src_pte,
	// 					 addr, rss, prealloc, page);
	// 	}
	// 	rss[mm_counter(page)]++;
	// } else if (page) {
	// 	get_page(page);
	// 	page_dup_file_rmap(page, false);
	// 	rss[mm_counter(page)]++;
	// }

	/*
	 * If it's a COW mapping, write protect it both
	 * in the parent and the child
	 */
	if (is_cow_mapping(vm_flags) && pte_write(pte)) {
		
						/* printk("Making page write protected\n");
						printk("pmd = %lx, addr = %lx\n", *(long unsigned int *)src_pmd, addr);
						printk("pte_before =\t0x%lx ", pte_offset_map(src_pmd, addr)->pte); */
		ptep_set_wrprotect(src_mm, addr, src_pte);
		//pte = pte_wrprotect(pte);
						/* printk("pte_after =\t0x%lx\n ", pte_offset_map(src_pmd, addr)->pte); */
	}
	//VM_BUG_ON(page && PageAnon(page) && PageAnonExclusive(page));

	/*
	 * If it's a shared mapping, mark it clean in
	 * the child
	 */
	// if (vm_flags & VM_SHARED)
	// 	pte = pte_mkclean(pte);
	// pte = pte_mkold(pte);

	// if (!userfaultfd_wp(dst_vma))
	// 	pte = pte_clear_uffd_wp(pte);

	// set_pte_at(dst_vma->vm_mm, addr, dst_pte, pte);
	return 0;
}

static inline int
walk_pte_range(/* struct vm_area_struct *dst_vma, */ struct vm_area_struct *src_vma,
	       /* pmd_t *dst_pmd, */ pmd_t *src_pmd, unsigned long addr,
	       unsigned long end)
{
	/* struct mm_struct *dst_mm = dst_vma->vm_mm; */
	struct mm_struct *src_mm = src_vma->vm_mm;
	pte_t *orig_src_pte/* , *orig_dst_pte */;
	pte_t *src_pte/* , *dst_pte */;
	spinlock_t *src_ptl/* , *dst_ptl */;
	int progress, ret = 0;
	//int rss[NR_MM_COUNTERS];
	// swp_entry_t entry = (swp_entry_t){0};
	//struct page *prealloc = NULL;
											/* trace_printk("\n");*/
again:
	progress = 0;
	// init_rss_vec(rss);

	/* dst_pte = pte_alloc_map_lock(dst_mm, dst_pmd, addr, &dst_ptl);
	if (!dst_pte) {
		ret = -ENOMEM;
		goto out;
	} */
	src_pte = pte_offset_map(src_pmd, addr);
	src_ptl = pte_lockptr(src_mm, src_pmd);
	spin_lock_nested(src_ptl, SINGLE_DEPTH_NESTING);
	orig_src_pte = src_pte;
	/* orig_dst_pte = dst_pte; */
	arch_enter_lazy_mmu_mode();

	do {
		/*
		 * We are holding two locks at this point - either of them
		 * could generate latencies in another task on another CPU.
		 */

        //progress related stuff, I am not concerned about it. I am bad! 
		/* if (progress >= 32) {
			progress = 0;
			if (need_resched() ||
			    spin_needbreak(src_ptl) || spin_needbreak(dst_ptl))
				break;
		} */
		if (pte_none(*src_pte)) {
			// progress++;
			continue;
		}
		if (unlikely(!pte_present(*src_pte))) {
			/* ret = copy_nonpresent_pte(dst_mm, src_mm,
						  dst_pte, src_pte,
						  dst_vma, src_vma,
						  addr, rss);
			if (ret == -EIO) {
				entry = pte_to_swp_entry(*src_pte);
				break;
			} else if (ret == -EBUSY) {
				break;
			} else if (!ret) {
				progress += 8;
				continue;
			} */
            										/* printk("hitting pte_not_present...\n"); */
            //don't exactly understand how it is working.

			/*
			 * Device exclusive entry restored, continue by copying
			 * the now present pte.
			 */
			WARN_ON_ONCE(ret != -ENOENT);
		}
		/* copy_present_pte() will clear `*prealloc' if consumed */
		ret = walk_present_pte(/* dst_vma,  */src_vma, /* dst_pte, */ src_pte,
				       addr/*, rss, &prealloc*/
					   /*This field is just for testing*/,src_pmd);
		/*
		 * If we need a pre-allocated page for this pte, drop the
		 * locks, allocate, and try again.
		 */
		if (unlikely(ret == -EAGAIN))
			break;
		// if (unlikely(prealloc)) {
		// 	/*
		// 	 * pre-alloc page cannot be reused by next time so as
		// 	 * to strictly follow mempolicy (e.g., alloc_page_vma()
		// 	 * will allocate page according to address).  This
		// 	 * could only happen if one pinned pte changed.
		// 	 */
		// 	put_page(prealloc);
		// 	prealloc = NULL;
		// }
		// progress += 8;
	} while (/* dst_pte++, */ src_pte++, addr += PAGE_SIZE, addr != end);

	arch_leave_lazy_mmu_mode();
	spin_unlock(src_ptl);
	pte_unmap(orig_src_pte);
	// add_mm_rss_vec(dst_mm, rss);
	// pte_unmap_unlock(orig_dst_pte, dst_ptl);
	// cond_resched();

	/* if (ret == -EIO) {
		VM_WARN_ON_ONCE(!entry.val);
		if (add_swap_count_continuation(entry, GFP_KERNEL) < 0) {
			ret = -ENOMEM;
			goto out;
		}
		entry.val = 0;
	} else if (ret == -EBUSY) {
		goto out;
	} else if (ret ==  -EAGAIN) {
		prealloc = page_copy_prealloc(src_mm, src_vma, addr);
		if (!prealloc)
			return -ENOMEM;
	} else if (ret) {
		VM_WARN_ON_ONCE(1);
	}
    */
	/* We've captured and resolved the error. Reset, try again. */
	ret = 0;

	if (addr != end)
		goto again;
/* out:
	if (unlikely(prealloc))
		put_page(prealloc); */
	return ret;
}


static inline int
walk_pmd_range(/* struct vm_area_struct *dst_vma, */ struct vm_area_struct *src_vma,
	       /* pud_t *dst_pud, */ pud_t *src_pud, unsigned long addr,
	       unsigned long end)
{
	/* struct mm_struct *dst_mm = dst_vma->vm_mm; */
	// struct mm_struct *src_mm = src_vma->vm_mm;
	pmd_t *src_pmd/* , *dst_pmd */;
	unsigned long next;
															/* trace_printk("\n");*/
	/* dst_pmd = pmd_alloc(dst_mm, dst_pud, addr);
	if (!dst_pmd)
		return -ENOMEM; */
	src_pmd = pmd_offset(src_pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		// Deals with huge page stuff, need to look into it later!!
        // if (is_swap_pmd(*src_pmd) || pmd_trans_huge(*src_pmd)
		// 	|| pmd_devmap(*src_pmd)) {
		// 	int err;
		// 	VM_BUG_ON_VMA(next-addr != HPAGE_PMD_SIZE, src_vma);
		// 	err = copy_huge_pmd(dst_mm, src_mm, dst_pmd, src_pmd,
		// 			    addr, dst_vma, src_vma);
		// 	if (err == -ENOMEM)
		// 		return -ENOMEM;
		// 	if (!err)
		// 		continue;
		// 	/* fall through */
		// }
		if (pmd_none_or_clear_bad(src_pmd))
			continue;
		if (walk_pte_range(/* dst_vma, */ src_vma, /* dst_pmd, */ src_pmd,
				   addr, next))
			return -ENOMEM;
	} while (/* dst_pmd++, */ src_pmd++, addr = next, addr != end);
	return 0;
}


static inline int
walk_pud_range(/* struct vm_area_struct *dst_vma,*/ struct vm_area_struct *src_vma,
	       /* p4d_t *dst_p4d ,*/ p4d_t *src_p4d, unsigned long addr,
	       unsigned long end)
{
	/* struct mm_struct *dst_mm = dst_vma->vm_mm; */
	// struct mm_struct *src_mm = src_vma->vm_mm;
	pud_t *src_pud/* , *dst_pud */;
	unsigned long next;
														/* trace_printk("\n"); */

	/* dst_pud = pud_alloc(dst_mm, dst_p4d, addr);
	if (!dst_pud)
		return -ENOMEM; */
	src_pud = pud_offset(src_p4d, addr);
	do {
		next = pud_addr_end(addr, end);
		/*  The following is based on huge pages, would look into it later!!
            if (pud_trans_huge(*src_pud) || pud_devmap(*src_pud)) { 
			int err;

			VM_BUG_ON_VMA(next-addr != HPAGE_PUD_SIZE, src_vma);
			err = copy_huge_pud(dst_mm, src_mm,
					    dst_pud, src_pud, addr, src_vma);
			if (err == -ENOMEM)
				return -ENOMEM;
			if (!err)
				continue;
			// fall through 
		} 
        */
		if (pud_none_or_clear_bad(src_pud))
			continue;
		if (walk_pmd_range(/* dst_vma, */ src_vma, /* dst_pud, */ src_pud,
				   addr, next))
			return -ENOMEM;
	} while (/* dst_pud++, */ src_pud++, addr = next, addr != end);
	return 0;
}

static inline int
walk_p4d_range(/* struct vm_area_struct *dst_vma, */ struct vm_area_struct *src_vma,
	       /* pgd_t *dst_pgd, */ pgd_t *src_pgd, unsigned long addr,
	       unsigned long end)
{
	// struct mm_struct *dst_mm = dst_vma->vm_mm;
	p4d_t *src_p4d; //*dst_p4d;
	unsigned long next;
														/* trace_printk("\n"); */

	// dst_p4d = p4d_alloc(dst_mm, dst_pgd, addr);
	// if (!dst_p4d)
	// 	return -ENOMEM;
	src_p4d = p4d_offset(src_pgd, addr);
	do {
		next = p4d_addr_end(addr, end);
		if (p4d_none_or_clear_bad(src_p4d))
			continue;
		if (walk_pud_range(/* dst_vma, */ src_vma, /* dst_p4d, */ src_p4d,
				   addr, next))
			return -ENOMEM;
	} while (src_p4d++, addr = next, addr != end);
	return 0;
}


static inline int 
traverse_page_range(/* struct vm_area_struct *dst_vma, */ struct vm_area_struct *src_vma)
{
	pgd_t *src_pgd/*,  *dst_pgd */;
	unsigned long next;
	unsigned long addr = src_vma->vm_start;
	unsigned long end = src_vma->vm_end;
	/* struct mm_struct *dst_mm = dst_vma->vm_mm; */
	struct mm_struct *src_mm = src_vma->vm_mm;
	struct mmu_notifier_range range;
	bool is_cow;
	int ret;
														/* trace_printk("\n"); */


	// if (!vma_needs_copy(dst_vma, src_vma))
	// 	return 0;

	// if (is_vm_hugetlb_page(src_vma))
	// 	return copy_hugetlb_page_range(dst_mm, src_mm, dst_vma, src_vma);

	// if (unlikely(src_vma->vm_flags & VM_PFNMAP)) {
	// 	/*
	// 	 * We do not free on error cases below as remove_vma
	// 	 * gets called on error from higher level routine
	// 	 */
	// 	ret = track_pfn_copy(src_vma);
	// 	if (ret)
	// 		return ret;
	// }

	/*
	 * We need to invalidate the secondary MMU mappings only when
	 * there could be a permission downgrade on the ptes of the
	 * parent mm. And a permission downgrade will only happen if
	 * is_cow_mapping() returns true.
	 */
	is_cow = is_cow_mapping(src_vma->vm_flags);

	if (is_cow) {
		mmu_notifier_range_init(&range, MMU_NOTIFY_PROTECTION_PAGE,
					0, src_vma, src_mm, addr, end);
		mmu_notifier_invalidate_range_start(&range);
		/*
		 * Disabling preemption is not needed for the write side, as
		 * the read side doesn't spin, but goes to the mmap_lock.
		 *
		 * Use the raw variant of the seqcount_t write API to avoid
		 * lockdep complaining about preemptibility.
		 */
		mmap_assert_write_locked(src_mm);
		raw_write_seqcount_begin(&src_mm->write_protect_seq);
	}

	ret = 0;
	/* dst_pgd = pgd_offset(dst_mm, addr); */
	src_pgd = pgd_offset(src_mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(src_pgd))
			continue;
		if (unlikely(walk_p4d_range(/* dst_vma, */ src_vma, /* dst_pgd, */ src_pgd,
					    addr, next))) {
			ret = -ENOMEM;
			break;
		}
	} while (/* dst_pgd++, */ src_pgd++, addr = next, addr != end);

	if (is_cow) {
		raw_write_seqcount_end(&src_mm->write_protect_seq);
		mmu_notifier_invalidate_range_end(&range);
	}
	return ret;
}

static inline
bool is_viable_vma(struct vm_area_struct* vma){
    struct mm_struct* mm = vma -> vm_mm;
    bool is_stack_vma, is_file_backed, is_code_vma;

    is_stack_vma = (vma->vm_start<=mm->start_stack
						&& mm->start_stack<=vma->vm_end);
    is_file_backed = (vma->vm_file != NULL);
    is_code_vma = (vma->vm_start == mm->start_code
                        && vma->vm_end == mm->end_code);
    
    return !(is_stack_vma || is_file_backed || is_code_vma); //heap_vma, file_vma.. these are there check later.
}

static inline 
void make_pages_rdonly(void){
    struct task_struct * p = current;
    struct mm_struct* mm = p -> mm;
    struct vm_area_struct *mpnt;
    // int is_cow;
    int i=0;


							/*	printk("stack_start = 0x%lx\n", mm->start_stack);
								printk("stack_vm = %ld\n", mm->stack_vm); */
    // need to check the requirement of locks. For the time being I am working without locks
    // the matter or the code sequence is mentioned in dup_mmap() in kernel/fork.c
    for (mpnt = mm->mmap; mpnt; mpnt = mpnt->vm_next) {
		// is_cow = is_cow_mapping(mpnt->vm_flags);
        // printk("VMA region = %d, is_cow = %d\n", i, is_cow);

        //  based on requirement like stack page or not, we need to
        //  appropriately continue this body for certain vmas
        if(is_viable_vma(mpnt))
	    {    
			// printk("VMA %d: vm_start = 0x%lx : vm_end = 0x%lx : viable = %s\n",
		 	// 							i, mpnt->vm_start, mpnt->vm_end,
		 	// 								is_viable_vma(mpnt)?"YES":"NO");  
			traverse_page_range(mpnt); 
		}
		// printk("VMA %d: vm_start = 0x%lx : vm_end = 0x%lx : viable = %s\n",
		// 								i, mpnt->vm_start, mpnt->vm_end,
		// 									is_viable_vma(mpnt)?"YES":"NO");
		i++;
	}
	flush_tlb_mm(mm);
}



static inline
int enable_checkpointing(void)
{
    struct task_struct * p = current;
    struct mm_struct* mm = p -> mm;
    // struct vm_area_struct *mpnt;
    // int is_cow;
    // int i=0;
	if(mm->checkpoint_enabled) 
	{
		//if a context already exists, then check pointing again is not allowed
		return -EINVAL;
	}
    mm->checkpoint_enabled = 1;   //when forking, make clear this field to zero. Or else actually COW
                                  // shall not happen.

    //We need to walk the entire page table and make the anonymous pages read only
    make_pages_rdonly();

    /*
    * Next we have to make appropriate changes in the page faulting handling code. So,
    * when that there is a write to a page, (which is now set read-only) [also check functional 
    * correctness, like, what shall happen if we chose to write to a page which was previously
    * marked read-only, need to check, which section handles that stuff.], we allocate a page
    * copy on write and add that struct page pointer to the dictionary in mm_struct. Need to 
    * check how to allocate the node of the dictionary and add that to the current list. Also,
    * make appropriate changes to make sure that minor faulted pages are not check pointed. Then 
    * that shall be a fundamental error.
    */
    return 0;
}


static inline
int restore_checkpointed_state(void)
{
	/*
	* For restoring the checkpoint, we shall traverse the dictionary maintained under the mm_struct
	* of the current process and take each entry and try to substitute the page table entry.
	*/
	struct task_struct * p = current;
    struct mm_struct* mm = p -> mm;
    struct checkpoint_dict *dict, *next;
	struct mmu_notifier_range range;
	pte_t entry;
    int i=0;

	if(!mm->checkpoint_enabled)
	{
		//if there is no checkpoint existing, then we cannot possibly restore anything
		return -EINVAL;
	}					
	

	for (dict=mm->dict; dict; dict=next) {
									
		i++;
		
		//delayacct_wpcopy_start();
		// if(mm->checkpoint_enabled)
		// {
		// 	printk("Old-Page mapcount: 1 = %d\n", page_mapcount(dict->page));
		// 	printk("Old-Page refcount: 1 = %d\n", page_ref_count(dict->page));
		// }
		
		mem_cgroup_charge(page_folio(dict->new_page), mm, GFP_KERNEL);

		cgroup_throttle_swaprate(dict->new_page, GFP_KERNEL);

		__SetPageUptodate(dict->new_page);

		mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, dict->vma, mm,
				dict->address & PAGE_MASK,
				(dict->address & PAGE_MASK) + PAGE_SIZE); 
		mmu_notifier_invalidate_range_start(&range);

		dict->curr_pte = pte_offset_map_lock(mm, dict->pmd, dict->address, &dict->ptl_ptr);
		flush_cache_page(dict->vma, dict->address, pte_pfn(*dict->curr_pte));

		// page_remove_rmap(dict->page, dict->vma, false);
		//set_page_count(dict->page, 1);
		//hence forth dict->page behaves like a new page but just having the required data
		
		
		entry = mk_pte(dict->new_page, dict->vma->vm_page_prot);		
		entry = pte_sw_mkyoung(entry);
		if (unlikely(dict->unshare)) {
			if (pte_soft_dirty(*dict->curr_pte))
				entry = pte_mksoft_dirty(entry);
			if (pte_uffd_wp(*dict->curr_pte))
				entry = pte_mkuffd_wp(entry);
		} else {
			entry = maybe_mkwrite(pte_mkdirty(entry), dict->vma);
		}
		
		
		ptep_clear_flush_notify(dict->vma, dict->address, dict->curr_pte);
		page_add_new_anon_rmap(dict->new_page, dict->vma, dict->address);
		lru_cache_add_inactive_or_unevictable(dict->new_page, dict->vma);

		//copy_user_highpage(dict->page_to_be_freed, dict->page, dict->address, dict->vma);

		set_pte_at_notify(mm, dict->address, dict->curr_pte, entry); // I think this statement should be
																				//protected by locks
		
		
		update_mmu_cache(dict->vma, dict->address, dict->curr_pte);

		page_remove_rmap(dict->old_page, dict->vma, false);
		pte_unmap_unlock(dict->curr_pte, dict->ptl_ptr);
		mmu_notifier_invalidate_range_only_end(&range);

							
		/* printk("dict->curr_pte now =\t0x%lx, 0x%lx\n", dict->curr_pte->pte, dict->checkpointed_pte.pte); */
		
		//   printk("---------------------------------For PFN = %lx---------------------------\n",
		//   													page_to_pfn(dict->page_to_be_freed));
		//   printk("Pre-Page_to_be_freed mapcount = %d\n", page_mapcount(dict->page_to_be_freed));
		//   printk("Pre-Page_to_be_freed refcount = %d\n", page_ref_count(dict->page_to_be_freed));
		//page_remove_rmap(dict->page_to_be_freed, dict->vma, false);
		
		//put_page(dict->page_to_be_freed);
		
		//free_swap_cache(dict->page_to_be_freed);
		//page_mapcount_reset(dict->page_to_be_freed);
		//set_page_count(dict->page_to_be_freed, 2);
		put_page(dict->old_page);
		
		// if(mm->checkpoint_enabled)
		// {
		// 	printk("Old-Page mapcount: 2 = %d\n", page_mapcount(dict->page));
		// 	printk("Old-Page refcount: 2 = %d\n", page_ref_count(dict->page));
		// }
		
		
		//delayacct_wpcopy_end();
		
		//__free_page(dict->page_to_be_freed); //<--- look into it formally
		next = dict->next;
		kfree(dict);
	}
	//printk("NR_ELEMENTS IN DICT = %d\n", i);
	mm->dict = NULL; //formally free each node later
	mm->checkpoint_enabled = 0;
	flush_tlb_mm(mm);
	return 0;
}

// void for_checking_purpose(void)
// {
// 	struct task_struct * p = current;
//     struct mm_struct* mm = p -> mm;
//     mm->checkpoint_enabled = 1;
// }

SYSCALL_DEFINE1(mmcontext, int , swtch){

	int ret;
    if(swtch == 0)
    {
        //enable checkpointing
		ret = enable_checkpointing();
    }
    else if (swtch == 1)
    {
        //restore the checkpointed state
		ret = restore_checkpointed_state();
    }else
	{
		//invalid parameter to our system_call
		ret = -EINVAL;
	}
    return ret;
}