/*
 * rewl_test.c
 *
 *  Created on: Apr 3, 2018
 *      Author: johan
 */

#include <stdio.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_queue.h>
#include <param/param_client.h>
#include <slash/slash.h>
#include <math.h>

#include <slash/optparse.h>
#include <slash/dflopt.h>

static char rewl_pull_buf[25];
static param_queue_t rewl_pull_q = {
	.buffer = rewl_pull_buf,
	.buffer_size = 25,
	.type = PARAM_QUEUE_TYPE_GET,
	.used = 0,
};

static int rewl_log_slash(struct slash *slash) {
	param_t * rpm = param_list_find_id(0, 140);
	param_queue_add(&rewl_pull_q, rpm, 0, NULL);
	float t = 0;
	printf("Logging REWL Data, press any key to interrupt\n");
	while(1) {

		param_pull_queue(&rewl_pull_q, 0, 0, 100);

		/* Delay (press enter to exit) */
		if (slash_wait_interruptible(slash, 100) != 0)
			break;

		printf("%f, %f\n", t, param_get_float(rpm));
		t += 0.1;

	};
	return SLASH_SUCCESS;
}

slash_command(rewl_log, rewl_log_slash, NULL, "Log REWL data");

int16_t sine(float t_sec, int amp, float freq)
{
	double x = freq * 2 * 3.1416 * t_sec;
	return (amp * sin(x));
}


static int rewl_sine_slash(struct slash *slash) {

	int node = slash_dfl_node;
    uint32_t amplitude = 0;
    float freq = 0;
	uint32_t rate = 100;

    optparse_t * parser = optparse_new("rewl_sine", "<amplitude> <freq>");
	optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_unsigned(parser, 'r', "rate", "NUM", 0, &rate, "update rate (default = 100 Hz)");
    optparse_add_help(parser);

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("missing amplitude parameter\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

    char * endptr;
    amplitude = strtoul(slash->argv[argi], &endptr, 10);

	if (++argi >= slash->argc) {
		printf("missing frequency parameter\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

    freq = strtod(slash->argv[argi], &endptr);

	printf("Applying amplitude of %u at %f Hz on node %u with a rate of %u\n", amplitude, freq, node, rate);

	param_t * amplitude_dist = param_list_find_id(node, 157);
	if (amplitude_dist == NULL) {
		printf("Could not find the amplitude_dist parameter on node %u\n", node);
		return SLASH_EINVAL;
	}

	int ms = 1000 / rate;

	float t_sec = 0.0;
	while(1) {
		if (slash_wait_interruptible(slash, ms) != 0)
			break;
		t_sec += 1.0 / (float)rate;
		int16_t disturbance = sine(t_sec, amplitude, freq);
		// printf("sine %d\n", disturbance);
		param_set_int16(amplitude_dist, disturbance);
		param_push_single(amplitude_dist, -1, NULL, 0, node, slash_dfl_timeout, 2);
	}


	return SLASH_SUCCESS;
}

slash_command(rewl_sine, rewl_sine_slash, NULL, "Apply sinusoidal offset to amplitude");