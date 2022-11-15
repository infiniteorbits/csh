#include <stdio.h>
#include <endian.h>

#include <slash/slash.h>
#include <slash/dflopt.h>
#include <slash/optparse.h>

#include <param/param_queue.h>
#include <param/param_client.h>
#include <param/param_server.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>

enum {
    MODE_DISABLED,
    MODE_CHARGE,
    MODE_CHARGE_WAIT,
    MODE_DISCHARGE,
    MODE_DISCHARGE_WAIT,
    MODE_STEPCHARGE,
    MODE_STEPCHARGE_WAIT,
    MODE_PULSE,
    MODE_PULSE_PAUSE,
    MODE_PULSE_WAIT,
    MODE_DONE_WAITING,
    MODE_DONE,
    MODE_MAX
};
char* str_mode[] = { 
    "disabled", 
    "charge", 
    "charge-wait", 
    "discharge", 
    "discharge-wait", 
    "stepcharge", 
    "stepcharge-wait", 
    "pulse", 
    "pulse-pause", 
    "pulse-wait", 
    "done-waiting", 
    "done"
};

const unsigned int NUM_BATS = 6;

static int battest_continue(struct slash *slash, unsigned int node, unsigned int timeout) {

	param_t * pmode = param_list_find_id(node, 178);
	param_t * pina_v = param_list_find_id(node, 175);
	param_t * pcycle_cnt = param_list_find_id(node, 177);
	param_t * pbat_id = param_list_find_id(node, 183);

    int nlines = 0;
    uint8_t done;
    uint16_t noresponse_cnt = 0;
    while (1) {
        done = true;
        if (param_pull_all(0, node, PM_TELEM, 0, timeout, 2)) {
            if (noresponse_cnt > 10) {
                printf("\x1b[2KNo response for the last %u seconds, battery is likely discharging to self-destruction. We should probably call somebody...\n", noresponse_cnt);
            }
            noresponse_cnt++;
            nlines = 0;
            continue;
        }

        for (int i = 0; i < NUM_BATS; i++) {
            uint8_t mode = param_get_uint8_array(pmode, i);
            if(mode != MODE_DISABLED && mode != MODE_DONE && mode != MODE_DONE_WAITING) {
                if (done && nlines > 0) {
                    printf("\x1b[%dA", nlines);
                    fflush(stdout);
                    done = false;
                    nlines = 0;
                }
                printf("\x1b[2KBattery %d running %s cycle %d at %u mV\n", i+1, str_mode[mode], param_get_uint8_array(pcycle_cnt, i)+1, param_get_uint16_array(pina_v, i));
                nlines++;
            }
        }

        if(done) {
            uint8_t file_exists = false;
            char file[256];
            strcat(strcpy(file, getenv("HOME")), "/bat.csv");
            FILE* csv = fopen(file, "r");
            if (csv != NULL) {
                file_exists = true;
                fclose(csv);
            }
            csv = fopen(file, "a");
            if (csv == NULL) {
                printf("Cannot create file %s\n", file);
                return SLASH_EIO;
            }
            if (!file_exists) {
                fprintf(csv, "BAT_ID,OCV0,OCV1,OCV2,OCV3,OCV4,OCV5,OCV6,POCV1,POCV2,PCCV1,PCCV2,PI1,PI2,CAP1,CAP2,Length,Diameter,Weight\n");
            }

            param_t * plength = param_list_find_id(node, 193);
            param_t * pdiameter = param_list_find_id(node, 194);
            param_t * pweight = param_list_find_id(node, 195);
            for (int i = 0; i < NUM_BATS; i++) {
                uint8_t mode = param_get_uint8_array(pmode, i);
                if (mode == MODE_DONE_WAITING) {
                    fprintf(csv, "%u", param_get_uint16_array(pbat_id, i));
                	param_t * pmeaspoints = param_list_find_id(node, 186+i);
                    for (int m = 0; m < pmeaspoints->array_size; m++) {
                        fprintf(csv, ",%u", param_get_uint16_array(pmeaspoints, m));
                    }
                    fprintf(csv, ",%u", param_get_uint16_array(plength, i));
                    fprintf(csv, ",%u", param_get_uint16_array(pdiameter, i));
                    fprintf(csv, ",%u", param_get_uint16_array(pweight, i));
                    fprintf(csv, "\n");
                    param_set_uint8_array(pmode, i, MODE_DONE);
                    printf("Stored measurements for battery %u\n", i+1);
                }
            }

            if (param_push_single(pmode, -1, NULL, 1, node, timeout, 2) < 0) {
                printf("No response\n");
                return SLASH_EIO;
            }

            fclose(csv);
            printf("Test is completed\n");
            break;
        }

		if (slash_wait_interruptible(slash, 1000) != 0) {
			break;
        }
    }

    return SLASH_SUCCESS;
}

static int battest_continue_slash(struct slash *slash) {

	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("hk retrieve", "timestamp");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "Battester address");

    if (param_list_find_id(node, 178) == NULL) {
        param_list_download(node, timeout, 2);
        if (param_list_find_id(node, 178) == NULL) {
            printf("Parameters not found, please check node ID and try again\n");
            return SLASH_EIO;
        }
    }

    return battest_continue(slash, node, timeout);
}
slash_command_sub(battest, continue, battest_continue_slash, NULL, NULL);

int inputmeas(struct slash *slash, uint8_t i, const char* measname, param_t* param, uint16_t min, uint16_t max) {

    printf("Input %s for battery %d (expected value between %u and %u):\n", measname, i+1, min, max);
    char * c = slash_readline(slash);
    if (c == NULL) {
        return -1;
    }
    int meas = atoi(c);
    if (meas > min && meas < max) {
        param_set_uint16_array(param, i, meas);
    } else {
        printf("Measurement outside usual limts, please repeat %s measurement:\n", measname);
        char * c = slash_readline(slash);
        if (c == NULL) {
            return -1;
        }
        int length = atoi(c);
        param_set_uint16_array(param, i, length);
    }

    return 0;
}

static char queue_buf[PARAM_SERVER_MTU];
static param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_SET, .version = 2 };

static int battest_start_slash(struct slash *slash) {

	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;

    optparse_t * parser = optparse_new("hk retrieve", "timestamp");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "Battester address");

	param_t * pmode = param_list_find_id(node, 178);
    if (pmode == NULL) {
        param_list_download(node, timeout, 2);
	    pmode = param_list_find_id(node, 178);
    }
	param_t * pbat_id = param_list_find_id(node, 183);
	param_t * plength = param_list_find_id(node, 193);
	param_t * pdiameter = param_list_find_id(node, 194);
	param_t * pweight = param_list_find_id(node, 195);

    if (pbat_id == NULL || pmode == NULL) {
        printf("Parameters not found, please check node ID and try again\n");
		return SLASH_EIO;
    }

    param_set_uint16(pbat_id, 0);

    for (int i = 0; i < NUM_BATS; i++) {
        printf("Input stock ID for battery %d (enter to skip):\n", i+1);
    	char * c = slash_readline(slash);
        if (c == NULL) {
            printf("Invalid ID: %s\n", c);
        }
        uint16_t bat_id = atoi(c);
        param_set_uint16_array(pbat_id, i, bat_id);
        if (bat_id != 0) {
    		param_set_uint8_array(pmode, i, MODE_CHARGE);
        }
    }

    for (int i = 0; i < NUM_BATS; i++) {
        if (param_get_uint16_array(pbat_id, i) != 0) {
            inputmeas(slash, i, "length", plength, 6430, 6530);
        }
    }

    for (int i = 0; i < NUM_BATS; i++) {
        if (param_get_uint16_array(pbat_id, i) != 0) {
            inputmeas(slash, i, "diameter", pdiameter, 1770, 1830);
        }
    }

    for (int i = 0; i < NUM_BATS; i++) {
        if (param_get_uint16_array(pbat_id, i) != 0) {
            inputmeas(slash, i, "weight", pweight, 44500, 45500);
        }
    }

    param_queue.used = 0;
    param_queue_add(&param_queue, pmode, -1, NULL);
    param_queue_add(&param_queue, pbat_id, -1, NULL);
    param_queue_add(&param_queue, plength, -1, NULL);
    param_queue_add(&param_queue, pdiameter, -1, NULL);
    param_queue_add(&param_queue, pweight, -1, NULL);

	printf("Type 's' + enter to continue:\n");
	char * c = slash_readline(slash);
    if (strcmp(c, "s") != 0) {
        printf("Abort\n");
        return SLASH_EUSAGE;
    }

    if (param_push_queue(&param_queue, 0, node, timeout, 0) < 0) {
        printf("No response\n");
        return SLASH_EIO;
    }

    printf("Test is running\n");
    
    battest_continue(slash, node, timeout);

    return SLASH_SUCCESS;
}
slash_command_sub(battest, start, battest_start_slash, NULL, NULL);
