
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "celix_errno.h"
#include "properties.h"
#include "hash_map.h"

#include "node_description_impl.h"
#include "node_description.h"
#include "node_description_writer.h"
#include "wiring_endpoint_description.h"


#define NODE_DESCRIPTION_ASSUMED_ENTRY_SIZE	256

#define NODE_DESCRIPTION_STRING_MAX_LEN 	16384


celix_status_t node_description_writer_nodeDescToString(node_description_pt inNodeDesc, char** outStr)
{
	celix_status_t status = CELIX_SUCCESS;

	char* ret = calloc(1, NODE_DESCRIPTION_STRING_MAX_LEN);

	if(ret==NULL){
		return CELIX_ENOMEM;
	}

	char* node_desc_str=ret;

	hash_map_iterator_pt node_props_it = hashMapIterator_create(inNodeDesc->properties);

	while(hashMapIterator_hasNext(node_props_it)){
		hash_map_entry_pt node_props_entry = hashMapIterator_nextEntry(node_props_it);
		char* key=(char*)hashMapEntry_getKey(node_props_entry);
		char* value=(char*)hashMapEntry_getValue(node_props_entry);
		int pair_len=strlen(key)+strlen(value)+3;
		snprintf(node_desc_str,pair_len,"%s=%s|",key,value);
		node_desc_str+=pair_len-1;
	}

	hashMapIterator_destroy(node_props_it);

	celixThreadMutex_lock(&inNodeDesc->wiring_ep_desc_list_lock);

	if(arrayList_size(inNodeDesc->wiring_ep_descriptions_list)>0){
		// Node Description properties done, let's go through wiring endpoint list
		ret[strlen(ret)-1]='#';


		array_list_iterator_pt ep_it = arrayListIterator_create(inNodeDesc->wiring_ep_descriptions_list);

		while(arrayListIterator_hasNext(ep_it)){
			wiring_endpoint_description_pt wep_desc = arrayListIterator_next(ep_it);
			int ep_base_len=strlen(WIRING_ENDPOINT_DESCRIPTION_URL_KEY)+strlen(wep_desc->url)+2;
			snprintf(node_desc_str,ep_base_len,"%s=%s",WIRING_ENDPOINT_DESCRIPTION_URL_KEY,wep_desc->url);
			node_desc_str+=ep_base_len-1;

			hash_map_iterator_pt wep_desc_props_it = hashMapIterator_create(wep_desc->properties);

			while(hashMapIterator_hasNext(wep_desc_props_it)){
				hash_map_entry_pt wep_desc_props_entry = hashMapIterator_nextEntry(wep_desc_props_it);
				char* key=(char*)hashMapEntry_getKey(wep_desc_props_entry);
				if(!strncmp(key,OSGI_RSA_ENDPOINT_FRAMEWORK_UUID,strlen(OSGI_RSA_ENDPOINT_FRAMEWORK_UUID))){
					continue;
				}
				char* value=(char*)hashMapEntry_getValue(wep_desc_props_entry);
				int ep_prop_len= 3 + strlen(key) + strlen(value);
				snprintf(node_desc_str,ep_prop_len,",%s=%s",key,value);
				node_desc_str+=ep_prop_len-1;
			}

			hashMapIterator_destroy(wep_desc_props_it);

			snprintf(node_desc_str,2,"!");
			node_desc_str++;

		}

		arrayListIterator_destroy(ep_it);
	}

	celixThreadMutex_unlock(&inNodeDesc->wiring_ep_desc_list_lock);

	ret[strlen(ret)-1]='\0';

	*outStr=ret;

	return status;
}



celix_status_t node_description_writer_stringToNodeDesc(char* inStr,node_description_pt* inNodeDesc){

	celix_status_t status = CELIX_SUCCESS;

	const char ep_list_delim[2]="#";
	const char node_props_delim[2]="|";
	const char ep_delim[2]="!";
	const char ep_props_delim[2]=",";
	const char field_delim[2]="=";

	nodeDescription_create(NULL,NULL,inNodeDesc);

	char* node_desc=strdup(inStr);

	char* node_props=NULL;
	char* ep_list=NULL;

	char* app=strtok(node_desc,ep_list_delim);
	if(app!=NULL){
		node_props=strdup(app);
	}

	app=strtok(NULL,ep_list_delim);
	if(app!=NULL){
		ep_list=strdup(app);
	}
	//printf("Node Description --> %s\n",node_props);
	//printf("Endpoint List --> %s\n",ep_list);


	char* saveptr1;
	char* saveptr2;
	char* saveptr3;
	char* token;

	if(node_props!=NULL){
		//Process node desc
		token=strtok_r(node_props,node_props_delim,&saveptr1);

		while(token!=NULL){
			char* key=strtok_r(token,field_delim,&saveptr2);
			char* value=strtok_r(NULL,field_delim,&saveptr2);
			if(!strncmp(key,NODE_DESCRIPTION_NODE_IDENTIFIER_KEY,strlen(NODE_DESCRIPTION_NODE_IDENTIFIER_KEY))){
				(*inNodeDesc)->nodeId=strdup(value);
			}

			properties_set((*inNodeDesc)->properties,key,value);

			token=strtok_r(NULL,node_props_delim,&saveptr1);
		}
	}

	//Process endpoint list
	if(ep_list!=NULL){
		token=strtok_r(ep_list,ep_delim,&saveptr1);

		while(token!=NULL){
			wiring_endpoint_description_pt wEndpointDescription = NULL;
			wiringEndpointDescription_create((*inNodeDesc)->nodeId,NULL,&wEndpointDescription);

			char* ep_token=strtok_r(token,ep_props_delim,&saveptr2);
			while(ep_token!=NULL){
				char* key=strtok_r(ep_token,field_delim,&saveptr3);
				char* value=strtok_r(NULL,field_delim,&saveptr3);
				if(!strncmp(key,WIRING_ENDPOINT_DESCRIPTION_URL_KEY,strlen(WIRING_ENDPOINT_DESCRIPTION_URL_KEY))){
					wEndpointDescription->url=strdup(value);
				}
				else if(!strncmp(key,OSGI_RSA_ENDPOINT_FRAMEWORK_UUID,strlen(OSGI_RSA_ENDPOINT_FRAMEWORK_UUID))){
					//Do nothing
				}
				else{
					/* Generic property*/
					properties_set(wEndpointDescription->properties,key,value);
				}

				ep_token=strtok_r(NULL,ep_props_delim,&saveptr2);
			}

			celixThreadMutex_lock(&((*inNodeDesc)->wiring_ep_desc_list_lock));
			arrayList_add((*inNodeDesc)->wiring_ep_descriptions_list,wEndpointDescription);
			celixThreadMutex_unlock(&((*inNodeDesc)->wiring_ep_desc_list_lock));

			token=strtok_r(NULL,ep_delim,&saveptr1);
		}
	}

	if(ep_list!=NULL){
		free(ep_list);
	}
	if(node_props!=NULL){
		free(node_props);
	}
	if(node_desc!=NULL){
		free(node_desc);
	}

	return status;

}
