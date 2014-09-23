/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2014 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Felipe Pena <felipe@php.net>                                |
   | Authors: Joe Watkins <joe.watkins@live.co.uk>                        |
   | Authors: Bob Weinand <bwoebi@php.net>                                |
   +----------------------------------------------------------------------+
*/

#include "php.h"
#include "phpdbg.h"
#include "phpdbg_utils.h"
#include "phpdbg_info.h"
#include "phpdbg_bp.h"
#include "phpdbg_prompt.h"

ZEND_EXTERN_MODULE_GLOBALS(phpdbg);

#define PHPDBG_INFO_COMMAND_D(f, h, a, m, l, s) \
	PHPDBG_COMMAND_D_EXP(f, h, a, m, l, s, &phpdbg_prompt_commands[14])

const phpdbg_command_t phpdbg_info_commands[] = {
	PHPDBG_INFO_COMMAND_D(break,    "show breakpoints",              'b', info_break,   NULL, 0),
	PHPDBG_INFO_COMMAND_D(files,    "show included files",           'F', info_files,   NULL, 0),
	PHPDBG_INFO_COMMAND_D(classes,  "show loaded classes",           'c', info_classes, NULL, 0),
	PHPDBG_INFO_COMMAND_D(funcs,    "show loaded classes",           'f', info_funcs,   NULL, 0),
	PHPDBG_INFO_COMMAND_D(error,    "show last error",               'e', info_error,   NULL, 0),
	PHPDBG_INFO_COMMAND_D(vars,     "show active variables",         'v', info_vars,    NULL, 0),
	PHPDBG_INFO_COMMAND_D(globals,  "show superglobals",             'g', info_globals, NULL, 0),
	PHPDBG_INFO_COMMAND_D(literal,  "show active literal constants", 'l', info_literal, NULL, 0),
	PHPDBG_INFO_COMMAND_D(memory,   "show memory manager stats",     'm', info_memory,  NULL, 0),
	PHPDBG_END_COMMAND
};

PHPDBG_INFO(break) /* {{{ */
{
	phpdbg_print_breakpoints(PHPDBG_BREAK_FILE TSRMLS_CC);
	phpdbg_print_breakpoints(PHPDBG_BREAK_SYM TSRMLS_CC);
	phpdbg_print_breakpoints(PHPDBG_BREAK_METHOD TSRMLS_CC);
	phpdbg_print_breakpoints(PHPDBG_BREAK_OPLINE TSRMLS_CC);
	phpdbg_print_breakpoints(PHPDBG_BREAK_FILE_OPLINE TSRMLS_CC);
	phpdbg_print_breakpoints(PHPDBG_BREAK_FUNCTION_OPLINE TSRMLS_CC);
	phpdbg_print_breakpoints(PHPDBG_BREAK_METHOD_OPLINE TSRMLS_CC);
	phpdbg_print_breakpoints(PHPDBG_BREAK_COND TSRMLS_CC);
	phpdbg_print_breakpoints(PHPDBG_BREAK_OPCODE TSRMLS_CC);

	return SUCCESS;
} /* }}} */

PHPDBG_INFO(files) /* {{{ */
{
	HashPosition pos;
	char *fname;

	phpdbg_notice("includedfilecount", "num=\"%d\"", "Included files: %d",
		zend_hash_num_elements(&EG(included_files)));

	zend_hash_internal_pointer_reset_ex(&EG(included_files), &pos);
	while (zend_hash_get_current_key_ex(&EG(included_files), &fname,
		NULL, NULL, 0, &pos) == HASH_KEY_IS_STRING) {
		phpdbg_writeln("includedfile", "name=\"%s\"", "File: %s", fname);
		zend_hash_move_forward_ex(&EG(included_files), &pos);
	}

	return SUCCESS;
} /* }}} */

PHPDBG_INFO(error) /* {{{ */
{
	if (PG(last_error_message)) {
		phpdbg_writeln("lasterror", "error=\"%s\" file=\"%s\" line=\"%d\"", "Last error: %s at %s line %d",
			PG(last_error_message), PG(last_error_file), PG(last_error_lineno));
	} else {
		phpdbg_notice("lasterror", "error=\"\"", "No error found!");
	}
	return SUCCESS;
} /* }}} */

static int phpdbg_arm_auto_global(zend_auto_global *auto_global TSRMLS_DC) {
	if (auto_global->armed) {
		auto_global->armed = auto_global->auto_global_callback(auto_global->name, auto_global->name_len TSRMLS_CC);
	}

	return 0;
}

static int phpdbg_print_symbols(zend_bool show_globals TSRMLS_DC) {
	HashTable vars, *symtable;
	HashPosition pos;
	char *var;
	zval **data;

	if (!EG(active_op_array)) {
		phpdbg_error("inactive", "type=\"op_array\"", "No active op array!");
		return SUCCESS;
	}

	if (!EG(active_symbol_table)) {
		zend_rebuild_symbol_table(TSRMLS_C);

		if (!EG(active_symbol_table)) {
			phpdbg_error("inactive", "type=\"symbol_table\"", "No active symbol table!");
			return SUCCESS;
		}
	}


	if (show_globals) {
		zend_hash_apply(CG(auto_globals), (apply_func_t) phpdbg_arm_auto_global TSRMLS_CC);
		symtable = &EG(symbol_table);
	} else {
		symtable = EG(active_symbol_table);
	}

	zend_hash_init(&vars, 8, NULL, NULL, 0);

	zend_hash_internal_pointer_reset_ex(symtable, &pos);
	while (zend_hash_get_current_key_ex(symtable, &var,
		NULL, NULL, 0, &pos) == HASH_KEY_IS_STRING) {
		zend_hash_get_current_data_ex(symtable, (void **)&data, &pos);
		if (zend_is_auto_global(var, strlen(var) TSRMLS_CC) ^ !show_globals) {
			zend_hash_update(&vars, var, strlen(var)+1, (void**)data, sizeof(zval*), NULL);
		}
		zend_hash_move_forward_ex(symtable, &pos);
	}

	if (show_globals) {
		phpdbg_notice("variableinfo", "count=\"%d\"", "Superglobal variables (%d)", zend_hash_num_elements(&vars));
	} else {
		zend_op_array *ops = EG(active_op_array);

		if (ops->function_name) {
			if (ops->scope) {
				phpdbg_notice("variableinfo", "method=\"%s::%s\" count=\"%d\"", "Variables in %s::%s() (%d)", ops->scope->name, ops->function_name, zend_hash_num_elements(&vars));
			} else {
				phpdbg_notice("variableinfo", "function=\"%s\" count=\"%d\"", "Variables in %s() (%d)", ops->function_name, zend_hash_num_elements(&vars));
			}
		} else {
			if (ops->filename) {
				phpdbg_notice("variableinfo", "file=\"%s\" count=\"%d\"", "Variables in %s (%d)", ops->filename, zend_hash_num_elements(&vars));
			} else {
				phpdbg_notice("variableinfo", "opline=\"%p\" count=\"%d\"", "Variables @ %p (%d)", ops, zend_hash_num_elements(&vars));
			}
		}
	}

	if (zend_hash_num_elements(&vars)) {
		phpdbg_out("Address\t\tRefs\tType\t\tVariable");
		for (zend_hash_internal_pointer_reset_ex(&vars, &pos);
			zend_hash_get_current_data_ex(&vars, (void**) &data, &pos) == SUCCESS;
			zend_hash_move_forward_ex(&vars, &pos)) {
			char *var;

			zend_hash_get_current_key_ex(&vars, &var, NULL, NULL, 0, &pos);

			if (*data) {
				phpdbg_writeln("variable", "address=\"%p\" refcount=\"%d\" type=\"%s\"", "%p\t%d\t(%s)", *data, Z_REFCOUNT_PP(data), zend_zval_type_name(*data));

				if (Z_TYPE_PP(data) == IS_RESOURCE) {
					int type;

					phpdbg_writeln("variabledetails", "refstatus=\"%s\" name=\"%s\" type=\"%s\"", "%s$%s\n|-------(typeof)------> (%s)\n", Z_ISREF_PP(data) ? "&": "", var, zend_list_find(Z_RESVAL_PP(data), &type) ? zend_rsrc_list_get_rsrc_type(type TSRMLS_CC) : "unknown");
				} else if (Z_TYPE_PP(data) == IS_OBJECT) {
					phpdbg_writeln("variabledetails", "refstatus=\"%s\" name=\"%s\" instanceof=\"%s\"", "%s$%s\n|-----(instanceof)----> (%s)\n", Z_ISREF_PP(data) ? "&": "", var, Z_OBJCE_PP(data)->name);
				} else {
					phpdbg_writeln("variabledetails", "refstatus=\"%s\" name=\"%s\"", "%s$%s", Z_ISREF_PP(data) ? "&": "", var);
				}
			} else {
				phpdbg_writeln("variable", "address=\"\" type=\"unknown\" name=\"%s\"", "n/a\tn/a\tn/a\t$%s", var);
			}
		}
	}

	zend_hash_destroy(&vars);

	return SUCCESS;
} /* }}} */

PHPDBG_INFO(vars) /* {{{ */
{
	return phpdbg_print_symbols(0 TSRMLS_CC);
}

PHPDBG_INFO(globals) /* {{{ */
{
	return phpdbg_print_symbols(1 TSRMLS_CC);
}

PHPDBG_INFO(literal) /* {{{ */
{
	if ((EG(in_execution) && EG(active_op_array)) || PHPDBG_G(ops)) {
		zend_op_array *ops = EG(active_op_array) ? EG(active_op_array) : PHPDBG_G(ops);
		int literal = 0, count = ops->last_literal-1;

		if (ops->function_name) {
			if (ops->scope) {
				phpdbg_notice("literalinfo", "method=\"%s::%s\" count=\"%d\"", "Literal Constants in %s::%s() (%d)", ops->scope->name, ops->function_name, count);
			} else {
				phpdbg_notice("literalinfo", "function=\"%s\" count=\"%d\"", "Literal Constants in %s() (%d)", ops->function_name, count);
			}
		} else {
			if (ops->filename) {
				phpdbg_notice("literalinfo", "file=\"%s\" count=\"%d\"", "Literal Constants in %s (%d)", ops->filename, count);
			} else {
				phpdbg_notice("literalinfo", "opline=\"%p\" count=\"%d\"", "Literal Constants @ %p (%d)", ops, count);
			}
		}

		while (literal < ops->last_literal) {
			if (Z_TYPE(ops->literals[literal].constant) != IS_NULL) {
				phpdbg_write("literal", "id=\"%u\"", "|-------- C%u -------> [", literal);
				zend_print_zval(
					&ops->literals[literal].constant, 0);
				phpdbg_out("]\n");
			}
			literal++;
		}
	} else {
		phpdbg_error("inactive", "type=\"execution\"", "Not executing!");
	}

	return SUCCESS;
} /* }}} */

PHPDBG_INFO(memory) /* {{{ */
{
	if (is_zend_mm(TSRMLS_C)) {
		phpdbg_notice("meminfo", "", "Memory Manager Information");
		phpdbg_notice("current", "", "Current");
		phpdbg_writeln("used", "mem=\"%.3f\"", "|-------> Used:\t%.3f kB", (float) (zend_memory_usage(0 TSRMLS_CC)/1024));
		phpdbg_writeln("real", "mem=\"%.3f\"", "|-------> Real:\t%.3f kB", (float) (zend_memory_usage(1 TSRMLS_CC)/1024));
		phpdbg_notice("peak", "", "Peak");
		phpdbg_writeln("used", "mem=\"%.3f\"", "|-------> Used:\t%.3f kB", (float) (zend_memory_peak_usage(0 TSRMLS_CC)/1024));
		phpdbg_writeln("real", "mem=\"%.3f\"", "|-------> Real:\t%.3f kB", (float) (zend_memory_peak_usage(1 TSRMLS_CC)/1024));
	} else {
		phpdbg_error("inactive", "type=\"memory_manager\"", "Memory Manager Disabled!");
	}
	return SUCCESS;
} /* }}} */

static inline void phpdbg_print_class_name(zend_class_entry **ce TSRMLS_DC) /* {{{ */
{
	phpdbg_writeln("class", "type=\"%s\" flags=\"%s\" name=\"%s\" methodcount=\"%d\"", "%s %s %s (%d)",
		((*ce)->type == ZEND_USER_CLASS) ?
			"User" : "Internal",
		((*ce)->ce_flags & ZEND_ACC_INTERFACE) ?
			"Interface" :
			((*ce)->ce_flags & ZEND_ACC_ABSTRACT) ?
				"Abstract Class" :
					"Class",
		(*ce)->name, zend_hash_num_elements(&(*ce)->function_table));
} /* }}} */

PHPDBG_INFO(classes) /* {{{ */
{
	HashPosition position;
	zend_class_entry **ce;
	HashTable classes;

	zend_hash_init(&classes, 8, NULL, NULL, 0);

	for (zend_hash_internal_pointer_reset_ex(EG(class_table), &position);
		zend_hash_get_current_data_ex(EG(class_table), (void**)&ce, &position) == SUCCESS;
		zend_hash_move_forward_ex(EG(class_table), &position)) {

		if ((*ce)->type == ZEND_USER_CLASS) {
			zend_hash_next_index_insert(&classes, ce, sizeof(ce), NULL);
		}
	}

	phpdbg_notice("classinfo", "count=\"%d\"", "User Classes (%d)", zend_hash_num_elements(&classes));

	for (zend_hash_internal_pointer_reset_ex(&classes, &position);
		zend_hash_get_current_data_ex(&classes, (void**)&ce, &position) == SUCCESS;
		zend_hash_move_forward_ex(&classes, &position)) {

		phpdbg_print_class_name(ce TSRMLS_CC);
		phpdbg_out("\n");

		if ((*ce)->parent) {
			phpdbg_xml("<parents>");
			zend_class_entry *pce = (*ce)->parent;
			do {
				phpdbg_out("|-------- ");
				phpdbg_print_class_name(&pce TSRMLS_CC);
			} while ((pce = pce->parent));
			phpdbg_xml("</parents>");
		}

		if ((*ce)->info.user.filename) {
			phpdbg_writeln("classsource", "file=\"\" line=\"%u\"", "|---- in %s on line %u",
				(*ce)->info.user.filename,
				(*ce)->info.user.line_start);
		} else {
			phpdbg_writeln("classsource", "", "|---- no source code");
		}
	    	phpdbg_out("\n");
	}

	zend_hash_destroy(&classes);

	return SUCCESS;
} /* }}} */

PHPDBG_INFO(funcs) /* {{{ */
{
	HashPosition position;
	zend_function *zf, **pzf;
	HashTable functions;

	zend_hash_init(&functions, 8, NULL, NULL, 0);

	for (zend_hash_internal_pointer_reset_ex(EG(function_table), &position);
		zend_hash_get_current_data_ex(EG(function_table), (void**)&zf, &position) == SUCCESS;
		zend_hash_move_forward_ex(EG(function_table), &position)) {

		if (zf->type == ZEND_USER_FUNCTION) {
			zend_hash_next_index_insert(
				&functions, (void**) &zf, sizeof(zend_function), NULL);
		}
	}

	phpdbg_notice("functioninfo", "count=\"%d\"", "User Functions (%d)", zend_hash_num_elements(&functions));

	for (zend_hash_internal_pointer_reset_ex(&functions, &position);
		zend_hash_get_current_data_ex(&functions, (void**)&pzf, &position) == SUCCESS;
		zend_hash_move_forward_ex(&functions, &position)) {
		zend_op_array *op_array = &((*pzf)->op_array);

		phpdbg_writeln("function", "name=\"%s\" file=\"%s\" line=\"%d\"", "|-------- %s in %s on line %d",
			op_array->function_name ? op_array->function_name : "{main}",
			op_array->filename ? op_array->filename : "(no source code)",
			op_array->line_start);
	}

	zend_hash_destroy(&functions);

	return SUCCESS;
} /* }}} */
