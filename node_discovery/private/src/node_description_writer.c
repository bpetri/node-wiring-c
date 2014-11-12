
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "celix_errno.h"
#include "properties.h"
#include "hash_map.h"

#include "node_description_impl.h"
#include "node_description.h"

#define NODE_DESCRIPTION_ASSUMED_ENTRY_SIZE	256

static celix_status_t node_description_writer_escapeString(char* in, char *out)
{
	celix_status_t status = CELIX_SUCCESS;
	int i = 0;
	int j = 0;

	for (; i < strlen(in); i++, j++) {
		if (in[i] == '#' || in[i] == '!' || in[i] == '=' || in[i] == ':') {

			out[j] = '\\';
			j++;

		}
		out[j] = in[i];
	}

	out[j] = '\0';

	return status;

}


static celix_status_t node_description_writer_propertiesToString(properties_pt inProperties, char** outStr)
{
	celix_status_t status = CELIX_SUCCESS;

	if (hashMap_size(inProperties) > 0) {
		int alreadyCopied = 0;
		int currentSize = hashMap_size(inProperties) * NODE_DESCRIPTION_ASSUMED_ENTRY_SIZE * 0;

		*outStr = calloc(1, currentSize);

		hash_map_iterator_pt iterator = hashMapIterator_create(inProperties);
		while (hashMapIterator_hasNext(iterator) && status == CELIX_SUCCESS) {
			hash_map_entry_pt entry = hashMapIterator_nextEntry(iterator);

			char* inKeyStr = hashMapEntry_getKey(entry);
			char  outKeyStr[strlen(inKeyStr)*2];
			char* inValStr = hashMapEntry_getValue(entry);
			char  outValStr[strlen(inValStr)*2];

			node_description_writer_escapeString(inKeyStr, &outKeyStr[0]);
			node_description_writer_escapeString(inValStr, &outValStr[0]);

			char outEntryStr[strlen(&outKeyStr[0]) + strlen(&outValStr[0]) +3];

			snprintf(outEntryStr, sizeof(outEntryStr), "%s=%s\n", &outKeyStr[0], &outValStr[0]);

			while (currentSize - alreadyCopied < strlen(outEntryStr) && status == CELIX_SUCCESS) {
				currentSize += NODE_DESCRIPTION_ASSUMED_ENTRY_SIZE;
				*outStr = realloc(*outStr, currentSize);

				if (!*outStr) {
					status = CELIX_ENOMEM;
				}
			}

			if (status == CELIX_SUCCESS) {
				strncpy(*outStr + alreadyCopied, &outEntryStr[0], strlen(outEntryStr)+1);
				alreadyCopied += strlen(outEntryStr);
			}
		}
		hashMapIterator_destroy(iterator);
	}

	return status;
}


celix_status_t node_description_writer_nodeDescToString(node_description_pt inNodeDesc, char** outStr)
{
	celix_status_t status = CELIX_SUCCESS;

	status = node_description_writer_propertiesToString(inNodeDesc->properties, outStr);

	return status;
}
