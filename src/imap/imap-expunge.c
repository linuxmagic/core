/* Copyright (c) 2003-2018 Dovecot authors, see the included COPYING file */

#include "imap-common.h"
#include "mail-storage.h"
#include "mail-search-build.h"
#include "imap-search-args.h"
#include "imap-expunge.h"

#define IMAP_EXPUNGE_BATCH_SIZE 1000

/* get a seqset of all the mails with \Deleted */
static int imap_expunge_get_seqset(struct mailbox *box,
				   struct mail_search_arg *next_search_arg,
				   ARRAY_TYPE(seq_range) *seqset)
{
	struct mailbox_transaction_context *t;
	struct mail_search_args *search_args;
	struct mail_search_context *ctx;
	struct mail *mail;
	int ret;

	search_args = mail_search_build_init();
	search_args->args = p_new(search_args->pool, struct mail_search_arg, 1);
	search_args->args->type = SEARCH_FLAGS;
	search_args->args->value.flags = MAIL_DELETED;
	search_args->args->next = next_search_arg;

	/* Refresh the flags so we'll expunge all messages marked as \Deleted
	   by any session. */
	t = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_REFRESH,
				      "EXPUNGE");
	ctx = mailbox_search_init(t, search_args, NULL, 0, NULL);

	/* collect the seqs into a seqset */
	while (mailbox_search_next(ctx, &mail))
		seq_range_array_add(seqset, mail->seq);

	ret = mailbox_search_deinit(&ctx);
	/* commit in case a plugin made changes - failures should not abort the expunge */
	(void) mailbox_transaction_commit(&t);
	mail_search_args_unref(&search_args);

	if (ret < 0)
		array_free(seqset);

	return ret;
}

int imap_expunge(struct mailbox *box, struct mail_search_arg *next_search_arg,
		 unsigned int *expunged_count)
{
	struct imap_search_seqset_iter *seqset_iter;
	struct mail_search_args *search_args;
	struct mailbox_status status;
	bool expunges = FALSE;
	int ret;

	if (mailbox_is_readonly(box)) {
		/* silently ignore */
		return 0;
	}

	mailbox_get_open_status(box, STATUS_MESSAGES, &status);

	search_args = mail_search_build_init();
	search_args->args = p_new(search_args->pool, struct mail_search_arg, 1);
	search_args->args->type = SEARCH_SEQSET;
	p_array_init(&search_args->args->value.seqset, search_args->pool, 16);

	if (imap_expunge_get_seqset(box, next_search_arg,
				    &search_args->args->value.seqset) < 0) {
		mail_search_args_unref(&search_args);
		return -1;
	}

	/* NOTE: This batching mainly helps when lazy_expunge is used. Without
	   it, it just quickly writes a bunch of expunge transactions. The
	   actual email deletion is done afterwards in mailbox_sync() for all
	   of the mails. This isn't ideal, but alternatives are a bit tricky
	   because mailbox_sync() can't be called for the SELECTed mailbox.

	   a) Simpler fix would be to open a new mailbox view which can be
	   synced, but this causes imapc to open another IMAP connection.
	   Although if it's done only for large expunge transactions it would
	   be less of an issue.

	   b) The caller would have to do batching and sync the mailbox
	   multiple times. This would require a new kind of cmd_sync() that
	   would send untagged replies but not the tagged reply. */
	seqset_iter = imap_search_seqset_iter_init(search_args, status.messages,
						   IMAP_EXPUNGE_BATCH_SIZE);

	do {
		struct mailbox_transaction_context *t;
		struct mail_search_context *ctx;
		struct mail *mail;

		t = mailbox_transaction_begin(box, 0, "EXPUNGE");
		ctx = mailbox_search_init(t, search_args, NULL, 0, NULL);

		while (mailbox_search_next(ctx, &mail)) {
			*expunged_count += 1;
			mail_expunge(mail);
			expunges = TRUE;
		}

		ret = mailbox_search_deinit(&ctx);
		/* If mailbox search fails, just commit the expunges done
		   so far. There's no need to rollback. */
		if (mailbox_transaction_commit(&t) < 0)
			ret = -1;
	} while (ret >= 0 && imap_search_seqset_iter_next(seqset_iter));

	imap_search_seqset_iter_deinit(&seqset_iter);
	mail_search_args_unref(&search_args);

	if (ret < 0)
		return ret;

	return expunges ? 1 : 0;
}
