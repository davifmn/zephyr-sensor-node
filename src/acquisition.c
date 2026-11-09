#include "acquisition.h"

#include "node.h"
#include "sensor_source.h"

int acquisition_step(struct k_msgq *queue, uint32_t seq)
{
	struct reading r = {.seq = seq, .status = 0, .milli_c = 0};
	int err;

	r.status = sensor_source_read(&r.milli_c);
	err = k_msgq_put(queue, &r, K_NO_WAIT);
	if (err != 0) {
		node_count_dropped();
	}
	return err;
}
