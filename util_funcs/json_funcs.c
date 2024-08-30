#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <json-c/json.h>

//my libraries
#include "json_funcs.h"
//size of the single buffer read, when reading a file to buffer
#define JSONFILESIZE 1024
//the size of string arrays, i.e const char *string_array[MAX_ATTRIB_ARRAY]
#define MAX_ATTRIB_ARRAY 200

static char* app_json_read_to_buffer(const char* file_path){
    char* ret_string = NULL;
    FILE* fp;
    fp = fopen(file_path, "r");
    if(!fp){
	return ret_string;
    }

    ret_string = (char*)malloc(sizeof(char)*JSONFILESIZE+1);
    if(!ret_string){
	goto clean;		
    }
    size_t read_num = fread(ret_string, sizeof(char), JSONFILESIZE, fp);
    int total_read = read_num;

    char* temp_string = NULL;
    while(read_num>=JSONFILESIZE){
	temp_string = realloc(ret_string, (total_read+read_num)*sizeof(char)+1);
	if(!temp_string){
	    goto clean;
	}
	ret_string = temp_string;
	read_num = fread(ret_string+total_read, sizeof(char), JSONFILESIZE, fp);
	total_read+=read_num;	 
    }
    temp_string = realloc(ret_string, total_read*sizeof(char)+1);
    //dont forget the null terminator
    temp_string[total_read] = '\0';
    if(!temp_string){
	goto clean;
    }
    ret_string = temp_string;

clean:
    if(fp)fclose(fp);
    return ret_string;
}
//TODO only for debuging for now
void app_json_print_name(JSONHANDLE* in_handle){
    struct json_object* parsed_fp = (struct json_object*)in_handle;
    if(!parsed_fp)return;
    printf("name %s \n", json_object_to_json_string(parsed_fp));
}

JSONHANDLE* app_json_iterate_and_return_parent(JSONHANDLE* in_handle, JSONHANDLE* child){
    struct json_object* parsed_fp = (struct json_object*)in_handle;
    if(!parsed_fp)return NULL;
    struct json_object* child_fp = (struct json_object*)child;
    if(!child_fp)return NULL;

    //iterate through the parsed_fp
    struct json_object_iterator it;
    struct json_object_iterator itEnd;
    it = json_object_iter_begin(parsed_fp);
    itEnd = json_object_iter_end(parsed_fp);
    while (!json_object_iter_equal(&it, &itEnd)) {
	struct json_object* rec_obj = NULL;
	rec_obj = json_object_iter_peek_value(&it);
	if(json_object_equal(child_fp,rec_obj)==1){	    
	    return (JSONHANDLE*)parsed_fp;
	}
	if(json_object_get_type(rec_obj)==json_type_object){
	    JSONHANDLE* parent = app_json_iterate_and_return_parent((JSONHANDLE*)rec_obj, child);
	    if(parent)return parent;
	}	
	json_object_iter_next(&it);
    }

    return NULL;
}

int app_json_iterate_and_find_obj(JSONHANDLE* in_handle, const char* find_name, JSONHANDLE*** objs, unsigned int* objs_size){
    struct json_object* parsed_fp = (struct json_object*)in_handle;
    if(!parsed_fp)return -1;
    if(!(*objs))return -1;
    
    struct json_object* ret_obj = NULL;
    
    //iterate through the parsed_fp
    struct json_object_iterator it;
    struct json_object_iterator itEnd;

    it = json_object_iter_begin(parsed_fp);
    itEnd = json_object_iter_end(parsed_fp);
    while (!json_object_iter_equal(&it, &itEnd)) {
	if(strcmp(json_object_iter_peek_name(&it), find_name)==0){	    
	    ret_obj = json_object_iter_peek_value(&it);
	    unsigned int cur_size = *objs_size;
	    JSONHANDLE** new_objs = realloc(*objs, sizeof(JSONHANDLE*) * (cur_size + 1));
	    if(!new_objs && *objs_size > 0)return -2;
	    if(!new_objs) return -1;
	    new_objs[cur_size] = ret_obj;
	    *objs = new_objs;
	    *objs_size += 1;
	}
	struct json_object* rec_obj = NULL;
	rec_obj = json_object_iter_peek_value(&it);
	if(json_object_get_type(rec_obj)==json_type_object){
	    int err = app_json_iterate_and_find_obj(rec_obj, find_name, objs, objs_size);
	    if(err < 0)return err;
	}
	json_object_iter_next(&it);
    }

    return 0;
}

int app_json_iterate_objs_run_callback(JSONHANDLE* in_handle,
				       const char* json_name, const char* json_parent, const char* top_name,
				       void* arg,
				       void(*proc_func)(void*, const char* js_name, const char* parent_name,
							const char* top_node_name, JSONHANDLE* found_handle_obj)){
    int return_val = 0;
    struct json_object* parsed_fp = (struct json_object*)in_handle;
    if(!parsed_fp){
	return_val = -1;
	return return_val;
    }
    //iterate through the parsed_fp
    struct json_object_iterator it;
    struct json_object_iterator itEnd;

    it = json_object_iter_begin(parsed_fp);
    itEnd = json_object_iter_end(parsed_fp);
    
    unsigned int iter = 0;
    while (!json_object_iter_equal(&it, &itEnd)) {
	const char* cur_name =  json_object_iter_peek_name(&it);
	struct json_object* rec_obj = NULL;
	rec_obj = json_object_iter_peek_value(&it);
	if(json_object_get_type(rec_obj)!=json_type_object && json_object_get_type(rec_obj)!=json_type_null){
	    iter+=1;
	    (*proc_func)(arg, json_name, json_parent, top_name, (JSONHANDLE*)rec_obj);
	}
	json_object_iter_next(&it);
    }
    //now go through the objects inside this json object
    //and call this function recursevily
    it = json_object_iter_begin(parsed_fp);
    while(!json_object_iter_equal(&it, &itEnd)){
	const char* cur_name =  json_object_iter_peek_name(&it);	
	struct json_object* rec_obj = NULL;
	const char* parent_name = json_name;
	if(iter<=0)parent_name = json_parent;
	rec_obj = json_object_iter_peek_value(&it);
	if(json_object_get_type(rec_obj)==json_type_object)
	    app_json_iterate_objs_run_callback((JSONHANDLE*)rec_obj, cur_name, parent_name, top_name,
					  arg, (*proc_func));
	json_object_iter_next(&it);
    }
    
    return return_val;
}

int app_json_create_obj(JSONHANDLE** obj){
    struct json_object* j_obj = json_object_new_object();
    *obj = j_obj;
    if(!obj)return -1;
    return 0;
}

JSONHANDLE* app_json_tokenise_path(const char* file_path){
    char* buffer = NULL;
    struct json_object* parsed_fp = NULL;
    
    buffer = app_json_read_to_buffer(file_path);
    if(!buffer){
	printf("no such path %s \n", file_path);
	return NULL;
    }
    parsed_fp = json_tokener_parse(buffer);
    free(buffer);
    if(!parsed_fp){
	printf("could not tokenize %s \n", file_path);
	return NULL;
    }

    return (JSONHANDLE*)parsed_fp;
}

int app_json_clean_object(JSONHANDLE* in_handle){
    struct json_object* obj = (struct json_object*)in_handle;
    if(!obj)return -1;

    json_object_put(obj);
    return 0;
}
