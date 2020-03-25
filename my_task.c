#include <vpi_user.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "timeout_rw.h"

typedef struct _calldata {
	int val;
	int vld;
	vpiHandle tdata;
	vpiHandle tvalid;
	vpiHandle tready;
} calldata;

typedef struct _ptr_to_free {
	void *free_me;
	struct _ptr_to_free *next;
} ptr_to_free;

static ptr_to_free head = {
	.free_me = NULL,
	.next = &head
};

void push_ptr_to_free(void *free_me) {
	ptr_to_free *node = malloc(sizeof(ptr_to_free));
	node->free_me = free_me;
	node->next = head.next;
	head.next = node;
}

void del_ptr_to_free_list() {
	ptr_to_free *cur = head.next;
	while (cur != &head) {
		if (cur->free_me) free(cur->free_me);
		ptr_to_free *next = cur->next;
		free(cur);
		cur = cur->next;
	}
}

static int my_entry_cb(s_cb_data *cb_data) {
	srand(time(NULL));
	//vpi_printf("In the entry callback\n");
	return 0;
}

static int my_exit_cb(s_cb_data *cb_data) {
	//vpi_printf("In the exit callback\n");
	del_ptr_to_free_list();
	return 0;
}

static int my_compiletf(char*user_data) {
	vpiHandle systfcall = vpi_handle(vpiSysTfCall, NULL);
	if (!systfcall) {
		vpi_printf("This doesn't work\n");
		return -1;
	}
	
	vpiHandle args_iter = vpi_iterate(vpiArgument, systfcall);
	vpiHandle tdata;
	vpiHandle tvalid;
	vpiHandle tready;
	
	tdata = vpi_scan(args_iter);
	if (!tdata) {
		vpi_printf("Wrong number of arguments; should use $test(data, valid, ready)\n");
		goto err;
	}
	tvalid = vpi_scan(args_iter);
	if (!tvalid) {
		vpi_printf("Wrong number of arguments; should use $test(data, valid, ready)\n");
		goto err;
	}
	tready = vpi_scan(args_iter);
	if (!tready) {
		vpi_printf("Wrong number of arguments; should use $test(data, valid, ready)\n");
		goto err;
	}
	
	if (vpi_scan(args_iter) != 0) {
		vpi_free_object(args_iter);
		vpi_printf("Wrong number of arguments; should use $test(data, valid, ready)\n");
		goto err;
	}
	
	if (vpi_get(vpiType, tdata) != vpiReg) {
		vpi_printf("Incorrect type for tdata argument; please use a register [type = %s]\n", vpi_get_str(vpiType, tdata));
		goto err;
	}
	if (vpi_get(vpiType, tvalid) != vpiReg) {
		vpi_printf("Incorrect type for tvalid argument; please use a register [type = %s]\n", vpi_get_str(vpiType, tvalid));
		goto err;
	}
	int tready_type = vpi_get(vpiType, tready);
	if (tready_type != vpiNet && tready_type != vpiReg && tready_type != vpiConstant) {
		vpi_printf("Incorrect type for tvalid argument; please use a wire (or a constant) [type = %s]\n", vpi_get_str(vpiType, tready));
		goto err;
	}
	
	calldata *ud = calloc(1, sizeof(calldata));
	push_ptr_to_free(ud);
	ud->tdata = tdata;
	ud->tvalid = tvalid;
	ud->tready = tready;
	vpi_put_userdata(systfcall, ud);
	return 0;
	
err:
	vpi_control(vpiFinish, 1);
	return -1;
}

static int my_calltf(char* user_data) {	  
	vpiHandle systfcall = vpi_handle(vpiSysTfCall, NULL);

	calldata *ud = vpi_get_userdata(systfcall);
	if (!ud) {
		vpi_printf("Some kind of fatal error occurred (%s could not get data)\n", __func__);
		vpi_control(vpiFinish, 1);
	} 
	
	s_vpi_value myval = {
		.format = vpiIntVal
	};
	
	//Check if a flit is being sent right now
	vpi_get_value(ud->tvalid, &myval);
	int current_vld = myval.value.integer;
	vpi_get_value(ud->tready, &myval);
	int current_rdy = myval.value.integer;
	
	if (current_vld && current_rdy) {
		ud->val++;
		//Decide if we will be valid on this cycle
		ud->vld = rand() & 1;
	} else if (ud->vld == 0) {
		//Decide if we will be valid on this cycle
		ud->vld = rand() & 1;
	}
	
	//I don't know if this will work, but I would like this task to be
	//"non-blocking". There will be small blips, but I view that as
	//acceptable
	s_vpi_time delay = {
		.type = vpiSimTime,
		.high = 0,
		.low = 1
	};
		
	//Output next tdata and tvalid
	myval.value.integer = ud->val;
	vpi_put_value(ud->tdata, &myval, &delay, vpiPureTransportDelay);
	
	myval.value.integer = ud->vld;
	vpi_put_value(ud->tvalid, &myval, &delay, vpiPureTransportDelay);
	
	return 0;
}

void my_task_register() {
	s_vpi_systf_data tf_data = {
		.type      = vpiSysTask,
		.tfname    = "$my_task",
		.calltf    = my_calltf,
		.compiletf = my_compiletf,
		.user_data = 0
	};
	
	static s_cb_data cb_data_entry = {
		.reason		= cbStartOfSimulation,
		.cb_rtn 	= my_entry_cb
	};
	
	static s_cb_data cb_data_exit = {
		.reason		= cbEndOfSimulation,
		.cb_rtn 	= my_exit_cb
	};
	
	vpi_register_systf(&tf_data);
	vpi_register_cb(&cb_data_entry);
	vpi_register_cb(&cb_data_exit);
}

void (*vlog_startup_routines[])() = {
	my_task_register,
	NULL
};
