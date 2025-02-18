/*
 * Copyright (C) 2012-2018 Vasily Tarasov
 * Copyright (C) 2012-2014 Geoff Kuenning
 * Copyright (C) 2012-2014 Sonam Mandal
 * Copyright (C) 2012-2014 Karthikeyani Palanisami
 * Copyright (C) 2012-2014 Philip Shilane
 * Copyright (C) 2012-2014 Sagar Trehan
 * Copyright (C) 2012-2018 Erez Zadok
 * Copyright (c) 2016-2017 Vinothkumar Raja
 * Copyright (c) 2017-2017 Nidhi Panpalia
 * Copyright (c) 2017-2018 Noopur Maheshwari
 * Copyright (c) 2018-2018 Rahul Rane
 * Copyright (c) 2012-2018 Stony Brook University
 * Copyright (c) 2012-2018 The Research Foundation for SUNY
 * This file is released under the GPL.
 */

#include "dm-dedup-target.h"
#include "dm-dedup-rw.h"
#include "dm-dedup-kvstore.h"

#define DMD_IO_SIZE	4096

/*
 * Given the block io sector,
 * it computes the sector number
 * and returns the lbn.
 */
static sector_t compute_sector(struct bio *bio,
			       struct dedup_config *dc)
{
	//printk(KERN_DEBUG "compute_sector\n");
	sector_t to_be_lbn;

	to_be_lbn = bio->bi_iter.bi_sector;
	(void) sector_div(to_be_lbn, dc->sectors_per_block);
	to_be_lbn *= dc->sectors_per_block;

	return to_be_lbn;
}

/*
 * Fetches whole block for the iorequest.
 *
 * Returns -ERR code in failure.
 * Returns 0 on success.
 */
static int fetch_whole_block(struct dedup_config *dc,
			     uint64_t pbn, struct page_list *pl)
{
	//printk(KERN_DEBUG "fetch_whole_block\n");
	struct dm_io_request iorq;
	struct dm_io_region where;
	unsigned long error_bits;

	where.bdev = dc->data_dev->bdev;
	where.sector = pbn;
	where.count = dc->sectors_per_block;

	iorq.bi_op = REQ_OP_READ;
	iorq.bi_op_flags = 0;
	iorq.mem.type = DM_IO_PAGE_LIST;
	iorq.mem.ptr.pl = pl;
	iorq.mem.offset = 0;
	iorq.notify.fn = NULL;
	iorq.client = dc->io_client;

	return dm_io(&iorq, 1, &where, &error_bits);
}

/*
 * Merges data of sectors.
 *
 * Returns -EINVAL code in failure.
 * Returns 0 on success.
 */
static int merge_data(struct dedup_config *dc, struct page *page,
		      struct bio *bio)
{
	//printk(KERN_DEBUG "merge_data\n");
	sector_t bi_sector = bio->bi_iter.bi_sector;
	void *src_page_vaddr, *dest_page_vaddr;
	int position, err = 0;
	struct bvec_iter iter;
	struct bio_vec bvec;

	/* Relative offset in terms of sector size */
	position = sector_div(bi_sector, dc->sectors_per_block);

	if (!page || !bio_page(bio)) {
		err = -EINVAL;
		goto out;
	}

	/* Locating the right sector to merge */
	dest_page_vaddr = page_address(page) + to_bytes(position);

	bio_for_each_segment(bvec, bio, iter) {
		src_page_vaddr = page_address(bio_iter_page(bio, iter)) + bio_iter_offset(bio, iter);

		/* Merging Data */
		memmove(dest_page_vaddr, src_page_vaddr, bio_iter_len(bio, iter));

		/* Updating destinaion address */
		dest_page_vaddr += bio_iter_len(bio, iter);
	}
out:
	return err;
}

static void copy_pages(struct page *src, struct bio *clone)
{
	//printk(KERN_DEBUG "copy_pages\n");
	void *src_page_vaddr, *dest_page_vaddr;

	src_page_vaddr = page_address(src);
	dest_page_vaddr = page_address(bio_page(clone));

	memmove(dest_page_vaddr, src_page_vaddr, DMD_IO_SIZE);
}

static void my_endio(struct bio *clone)
{
	//printk(KERN_DEBUG "my_endio\n");
	unsigned rw = bio_data_dir(clone);
	struct bio *orig;
	struct bio_vec *bv;

	/*if (!error && !bio_flagged(clone, BIO_UPTODATE))
		error = -EIO;*/

	/* free the processed pages */
	if (rw == WRITE || rw == READ || rw == REQ_OP_WRITE) {
		bv = clone->bi_io_vec;
		if (bv->bv_page) {
			//DMINFO("\nFreeing %llx",(unsigned long)page_address(bv->bv_page));
			//free_pages((unsigned long)page_address(bv->bv_page), 0);
			bv->bv_page = NULL;
		}
	}

	orig = clone->bi_private;
	orig->bi_status = BLK_STS_OK;
	bio_endio(orig);

	bio_put(clone);
}

/*
 * It allocates and initializes the bio structure.
 *
 * Returns NULL in failure.
 * Returns the valid pointer to struct bio on success.
 */
static struct bio *create_bio(struct dedup_config *dc,
			      struct bio *bio)
{
	//printk(KERN_DEBUG "create_bio\n");
	struct bio *clone;
	struct page *page;

	clone = bio_kmalloc(GFP_NOIO, 1);
	if (!clone)
		goto out;
	clone->bi_disk = bio->bi_disk;
	clone->bi_partno = bio->bi_partno;
	clone->bi_opf = bio->bi_opf;

	clone->bi_iter.bi_sector = compute_sector(bio, dc);
	clone->bi_private = bio;  /* for later completion */
	clone->bi_end_io = my_endio;

	page = alloc_pages(GFP_NOIO, 0);
        //DMINFO("\nallocated page crete bio %llx",(unsigned long)page_address(page));

	if (!page)
		goto bad_putbio;

	if (!bio_add_page(clone, page, DMD_IO_SIZE, 0))
		goto bad_freepage;

	goto out;

bad_freepage:
	free_pages((unsigned long) page_address(page), 0);

bad_putbio:
	bio_put(clone);
	//DMINFO("Assigning clone == NULL");
	clone = NULL;

out:
	return clone;
}

/*
 * Main function for handling bio.
 *
 * Returns NULL in failure.
 * Returns the valid pointer to struct bio on success.
 */
static struct bio *prepare_bio_with_pbn(struct dedup_config *dc,
					struct bio *bio, uint64_t pbn)
{
	//printk(KERN_DEBUG "prepare_bio_with_pbn\n");
	int r = 0;
	struct page_list *pl;
	struct bio *clone = NULL;

	pl = kmalloc(sizeof(*pl), GFP_NOIO);
	if (!pl)
		goto out;

	/*
	 * Since target I/O size is 4KB currently, we need only one page to
	 * store the data. However, if the target I/O size increases, we need
	 * to allocate more pages and set this linked list correctly.
	 */
	pl->page = alloc_pages(GFP_NOIO, 0);
	if (!pl->page)
		goto out_allocfail;

	pl->next = NULL;

	r = fetch_whole_block(dc, pbn, pl);
	if (r < 0)
		goto out_fail;

	r = merge_data(dc, pl->page, bio);
	if (r < 0)
		goto out_fail;

	clone = create_bio(dc, bio);
	if (!clone)
		goto out_fail;

	copy_pages(pl->page, clone);

out_fail:
	free_pages((unsigned long) page_address(pl->page), 0);
out_allocfail:
	kfree(pl);
out:
	if (r < 0)
		return ERR_PTR(r);

	return clone;
}

static struct bio *prepare_bio_without_pbn(struct dedup_config *dc,
					   struct bio *bio)
{
	//printk(KERN_DEBUG "prepare_bio_without_pbn\n");
	int r = 0;
	struct bio *clone = NULL;

	clone = create_bio(dc, bio);
	if (!clone)
		goto out;

	zero_fill_bio(clone);

	r = merge_data(dc, bio_page(clone), bio);
	if (r < 0)
		return ERR_PTR(r);
out:
	return clone;
}

/*
 * Wrapper function for prepare_bio_with_pbn and
 * prepare_bio_without_pbn.
 *
 * Returns ERR_PTR in failure.
 * Returns the valid pointer to struct bio on success.
 */
struct bio *prepare_bio_on_write(struct dedup_config *dc, struct bio *bio)
{
	//printk(KERN_DEBUG "prepare_bio_on_write\n");
	int r;
	sector_t lbn;
	uint32_t vsize;
	struct lbn_pbn_value lbnpbn_value;
	struct bio *clone;
	//DMINFO("\nEntered prepare bio on write");
	lbn = compute_sector(bio, dc);
	(void) sector_div(lbn, dc->sectors_per_block);

	/* check for old or new lbn and fetch the appropriate pbn */
	r = dc->kvs_lbn_pbn->kvs_lookup(dc->kvs_lbn_pbn, (void *)&lbn,
					sizeof(lbn), (void *)&lbnpbn_value, &vsize);
	if (r == -ENODATA)
		clone = prepare_bio_without_pbn(dc, bio);
	else if (r == 0)
		clone = prepare_bio_with_pbn(dc, bio,
					     lbnpbn_value.pbn * dc->sectors_per_block);
	else
		return ERR_PTR(r);
	//DMINFO("\nExiting prpare_bio_on_write");
	return clone;
}
