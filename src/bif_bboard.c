#include <stdlib.h>
#include <stdio.h>

#include "module.h"
#include "prolog.h"
#include "query.h"

#define is_smallatomic(c) (is_atom(c) || is_smallint(c))

#define DO_DUMP 0

#define DUMP_TERM2(s,k,c,c_ctx,running) { \
	fprintf(stderr, "%s %s ", s, k); \
	q->nl = true; q->quoted = true; \
	print_term(q, stderr, c, c_ctx, running); \
	q->nl = false; q->quoted = false; \
}

static bool bif_bb_b_put_2(query *q)
{
	GET_FIRST_ARG(p1,nonvar);
	GET_NEXT_ARG(p2,any);

	if (is_compound(p1) &&
		((p1->val_off != g_colon_s) || (p1->arity != 2)))
		return throw_error(q, p1, p1_ctx, "type_error", "callable");

	module *m;
	char tmpbuf[1024];

	if (is_compound(p1)) {
		cell *p1_m = p1 + 1;
		p1 = p1_m + p1_m->num_cells;

		if (!is_atom(p1_m) || !is_smallatomic(p1))
			return throw_error(q, p1, p1_ctx, "type_error", "atom");

		m = find_module(q->pl, C_STR(q, p1_m));

		if (!m)
			return throw_error(q, p1_m, p1_ctx, "existence_error", "module");
	} else
		m = q->pl->global_bb ? q->pl->user_m : q->st.m;

	if (is_atom(p1))
		snprintf(tmpbuf, sizeof(tmpbuf), "%s:%s:b", m->name, C_STR(q, p1));
	else
		snprintf(tmpbuf, sizeof(tmpbuf), "%s:%d:b", m->name, (int)get_smallint(p1));

	if (DO_DUMP) DUMP_TERM2("bb_b_put", tmpbuf, p2, p2_ctx, 1);

	cell key;
	make_atom(&key, new_atom(q->pl, tmpbuf));
	CHECKED(init_tmp_heap(q));
	cell *tmp = copy_term_to_tmp(q, p2, p2_ctx, false);
	CHECKED(tmp);
	pl_idx num_cells = tmp->num_cells;
	cell *val = malloc(sizeof(cell)*num_cells);
	CHECKED(val);
	dup_cells(val, tmp, tmp->num_cells);

	int var_num = create_vars(q, 1);
	CHECKED(var_num != -1);

	cell c, v;
	make_ref(&c, var_num, q->st.curr_fp);
	blob *b = calloc(1, sizeof(blob));
	b->ptr = (void*)m;
	b->ptr2 = (void*)strdup(tmpbuf);
	make_kvref(&v, b);

	if (!unify(q, &c, q->st.curr_fp, &v, q->st.curr_fp))
		return false;

	cell val_ptr_cell;
	val_ptr_cell.tag = TAG_EMPTY;
	val_ptr_cell.val_ptr = val;

	prolog_lock(q->pl);
	hash_set(q->pl->keyval, q->pl, &key, &val_ptr_cell);
	prolog_unlock(q->pl);

	return true;
}

static bool bif_bb_put_2(query *q)
{
	GET_FIRST_ARG(p1,nonvar);
	GET_NEXT_ARG(p2,any);

	if (is_compound(p1) &&
		((p1->val_off != g_colon_s) || (p1->arity != 2)))
		return throw_error(q, p1, p1_ctx, "type_error", "callable");

	module *m;
	char tmpbuf1[1024], tmpbuf2[1024];

	if (is_compound(p1)) {
		cell *p1_m = p1 + 1;
		p1 = p1_m + p1_m->num_cells;

		if (!is_atom(p1_m) || !is_smallatomic(p1))
			return throw_error(q, p1, p1_ctx, "type_error", "atom");

		m = find_module(q->pl, C_STR(q, p1_m));

		if (!m)
			return throw_error(q, p1_m, p1_ctx, "existence_error", "module");
	} else
		m = q->pl->global_bb ? q->pl->user_m : q->st.m;

	if (is_atom(p1))
		snprintf(tmpbuf1, sizeof(tmpbuf1), "%s:%s:b", m->name, C_STR(q, p1));
	else
		snprintf(tmpbuf1, sizeof(tmpbuf1), "%s:%d:b", m->name, (int)get_smallint(p1));

	cell key1;
	make_atom(&key1, new_atom(q->pl, tmpbuf1));

	if (is_atom(p1))
		snprintf(tmpbuf2, sizeof(tmpbuf2), "%s:%s", m->name, C_STR(q, p1));
	else
		snprintf(tmpbuf2, sizeof(tmpbuf2), "%s:%d", m->name, (int)get_smallint(p1));

	if (DO_DUMP) DUMP_TERM2("bb_put", tmpbuf2, p2, p2_ctx, 1);

	cell key2;
	make_atom(&key2, new_atom(q->pl, tmpbuf2));
	CHECKED(init_tmp_heap(q));
	cell *tmp = copy_term_to_tmp(q, p2, p2_ctx, false);
	CHECKED(tmp);
	pl_idx num_cells = tmp->num_cells;
	cell *val = malloc(sizeof(cell)*num_cells);
	CHECKED(val);
	dup_cells(val, tmp, tmp->num_cells);

	cell val_ptr_cell;
	val_ptr_cell.tag = TAG_EMPTY;
	val_ptr_cell.val_ptr = val;

	prolog_lock(q->pl);
	hash_del(q->pl->keyval, q->pl, &key1);
	hash_del(q->pl->keyval, q->pl, &key2);
	hash_set(q->pl->keyval, q->pl, &key2, &val_ptr_cell);
	prolog_unlock(q->pl);

	return true;
}

static bool bif_bb_get_2(query *q)
{
	GET_FIRST_ARG(p1,nonvar);

	if (is_compound(p1) &&
		((p1->val_off != g_colon_s) || (p1->arity != 2)))
		return throw_error(q, p1, p1_ctx, "type_error", "callable");

	module *m;
	char tmpbuf[1024];

	if (is_compound(p1)) {
		cell *p1_m = p1 + 1;
		p1 = p1_m + p1_m->num_cells;

		if (!is_atom(p1_m) || !is_smallatomic(p1))
			return throw_error(q, p1, p1_ctx, "type_error", "atom");

		m = find_module(q->pl, C_STR(q, p1_m));

		if (!m)
			return throw_error(q, p1_m, p1_ctx, "existence_error", "module");
	} else
		m = q->pl->global_bb ? q->pl->user_m : q->st.m;

	if (is_atom(p1))
		snprintf(tmpbuf, sizeof(tmpbuf), "%s:%s:b", m->name, C_STR(q, p1));
	else
		snprintf(tmpbuf, sizeof(tmpbuf), "%s:%d:b", m->name, (int)get_smallint(p1));

	cell key;
	make_atom(&key, new_atom(q->pl, tmpbuf));
	cell val_cell;

	prolog_lock(q->pl);

	if (!hash_get(q->pl->keyval, q->pl, &key, &val_cell)) {
		if (is_atom(p1))
			snprintf(tmpbuf, sizeof(tmpbuf), "%s:%s", m->name, C_STR(q, p1));
		else
			snprintf(tmpbuf, sizeof(tmpbuf), "%s:%d", m->name, (int)get_smallint(p1));

		make_atom(&key, new_atom(q->pl, tmpbuf));

		if (!hash_get(q->pl->keyval, q->pl, &key, &val_cell)) {
			prolog_unlock(q->pl);
			return false;
		}
	}

	prolog_unlock(q->pl);

	CHECKED(check_frame(q, MAX_ARITY));
	try_me(q, MAX_ARITY);
	cell *tmp = copy_term_to_heap(q, (cell*)val_cell.val_ptr, q->st.fp, false);
	CHECKED(tmp);
	GET_FIRST_ARG(p1x,nonvar);
	GET_NEXT_ARG(p2,any);

	if (DO_DUMP) DUMP_TERM2("bb_get", tmpbuf, tmp, q->st.curr_fp, 1);

	if (is_var(p2) && is_var(tmp)) {
		const frame *f = GET_FRAME(q->st.curr_fp);
		const slot *e = get_slot(q, f, tmp->var_num);
		const frame *f2 = GET_FRAME(p2_ctx);
		slot *e2 = get_slot(q, f2, p2->var_num);
		*e2 = *e;
		return true;
	}

	return unify(q, p2, p2_ctx, tmp, q->st.curr_fp);
}

static bool bif_bb_delete_2(query *q)
{
	GET_FIRST_ARG(p1,nonvar);

	if (is_compound(p1) &&
		((p1->val_off != g_colon_s) || (p1->arity != 2)))
		return throw_error(q, p1, p1_ctx, "type_error", "callable");

	module *m;
	char tmpbuf[1024];

	if (is_compound(p1)) {
		cell *p1_m = p1 + 1;
		p1 = p1_m + p1_m->num_cells;

		if (!is_atom(p1_m) || !is_smallatomic(p1))
			return throw_error(q, p1, p1_ctx, "type_error", "atom");

		m = find_module(q->pl, C_STR(q, p1_m));

		if (!m)
			return throw_error(q, p1_m, p1_ctx, "existence_error", "module");
	} else
		m = q->pl->global_bb ? q->pl->user_m : q->st.m;

	if (is_atom(p1))
		snprintf(tmpbuf, sizeof(tmpbuf), "%s:%s", m->name, C_STR(q, p1));
	else
		snprintf(tmpbuf, sizeof(tmpbuf), "%s:%d", m->name, (int)get_smallint(p1));

	cell key;
	make_atom(&key, new_atom(q->pl, tmpbuf));
	cell val_cell;

	prolog_lock(q->pl);

	if (!hash_get(q->pl->keyval, q->pl, &key, &val_cell)) {
		prolog_unlock(q->pl);
		return false;
	}

	CHECKED(check_frame(q, MAX_ARITY), prolog_unlock(q->pl));
	try_me(q, MAX_ARITY);
	cell *tmp = copy_term_to_heap(q, (cell*)val_cell.val_ptr, q->st.fp, false);
	CHECKED(tmp, prolog_unlock(q->pl));
	GET_FIRST_ARG(p1x,nonvar);
	GET_NEXT_ARG(p2,any);

	if (DO_DUMP) DUMP_TERM2("bb_delete", tmpbuf, tmp, q->st.curr_fp, 1);

	if (is_var(p2) && is_var(tmp)) {
		const frame *f = GET_FRAME(q->st.curr_fp);
		const slot *e = get_slot(q, f, tmp->var_num);
		const frame *f2 = GET_FRAME(p2_ctx);
		slot *e2 = get_slot(q, f2, p2->var_num);
		*e2 = *e;
		bool ok = hash_del(q->pl->keyval, q->pl, &key);
		prolog_unlock(q->pl);
		return ok;
	}

	if (!unify(q, p2, p2_ctx, tmp, q->st.curr_fp)) {
		prolog_unlock(q->pl);
		return false;
	}

	bool ok = hash_del(q->pl->keyval, q->pl, &key);
	prolog_unlock(q->pl);
	return ok;
}

static bool bif_bb_update_3(query *q)
{
	GET_FIRST_ARG(p1,nonvar);

	if (is_compound(p1) &&
		((p1->val_off != g_colon_s) || (p1->arity != 2)))
		return throw_error(q, p1, p1_ctx, "type_error", "callable");

	module *m;
	char tmpbuf[1024];

	if (is_compound(p1)) {
		cell *p1_m = p1 + 1;
		p1 = p1_m + p1_m->num_cells;

		if (!is_atom(p1_m) || !is_smallatomic(p1))
			return throw_error(q, p1, p1_ctx, "type_error", "atom");

		m = find_module(q->pl, C_STR(q, p1_m));

		if (!m)
			return throw_error(q, p1_m, p1_ctx, "existence_error", "module");
	} else
		m = q->pl->global_bb ? q->pl->user_m : q->st.m;

	if (is_atom(p1))
		snprintf(tmpbuf, sizeof(tmpbuf), "%s:%s", m->name, C_STR(q, p1));
	else
		snprintf(tmpbuf, sizeof(tmpbuf), "%s:%d", m->name, (int)get_smallint(p1));

	cell key;
	make_atom(&key, new_atom(q->pl, tmpbuf));
	cell val_cell;

	prolog_lock(q->pl);

	if (!hash_get(q->pl->keyval, q->pl, &key, &val_cell)) {
		prolog_unlock(q->pl);
		return false;
	}

	CHECKED(check_frame(q, MAX_ARITY), prolog_unlock(q->pl));
	try_me(q, MAX_ARITY);
	q->noderef = true;
	cell *tmp = copy_term_to_heap(q, (cell*)val_cell.val_ptr, q->st.fp, false);
	q->noderef = false;
	CHECKED(tmp, prolog_unlock(q->pl));
	GET_FIRST_ARG(p1x,nonvar);
	GET_NEXT_ARG(p2,any);
	GET_NEXT_ARG(p3,any);

	if (DO_DUMP) DUMP_TERM2("bb_update", tmpbuf, p2, p2_ctx, 1);

	if (!unify(q, p2, p2_ctx, tmp, q->st.curr_fp)) {
		prolog_unlock(q->pl);
		return false;
	}

	tmp = copy_term_to_heap(q, p3, p3_ctx, false);
	CHECKED(tmp, prolog_unlock(q->pl));
	cell *value = malloc(sizeof(cell)*tmp->num_cells);
	CHECKED(value, prolog_unlock(q->pl));
	dup_cells(value, tmp, tmp->num_cells);

	cell val_ptr_cell;
	val_ptr_cell.tag = TAG_EMPTY;
	val_ptr_cell.val_ptr = value;

	hash_set(q->pl->keyval, q->pl, &key, &val_ptr_cell);

	prolog_unlock(q->pl);

	return true;
}

void bb_erase(module *m, const char *ref)
{
	cell key;
	make_atom(&key, new_atom(m->pl, ref));
	prolog_lock(m->pl);
	hash_del(m->pl->keyval, m->pl, &key);
	prolog_unlock(m->pl);
}

builtins g_bboard_bifs[] =
{
	{"bb_b_put", 2, bif_bb_b_put_2, ":atom,+term", false, false, BLAH},
	{"bb_put", 2, bif_bb_put_2, ":atom,+term", false, false, BLAH},
	{"bb_get", 2, bif_bb_get_2, ":atom,?term", false, false, BLAH},

	{"bb_update", 3, bif_bb_update_3, ":atom,?term,?term", false, false, BLAH},
	{"bb_delete", 2, bif_bb_delete_2, ":atom,?term", false, false, BLAH},

	{0}
};
