#pragma once
//handle for building json objects from external functions
typedef void JSONHANDLE;
//load a file to the buffer and return it, needs to be freed later
static char* app_json_read_to_buffer(const char* file_path);
//iterate recursively through the json object and find a key and return the object of that key, value pair
JSONHANDLE* app_json_iterate_and_find_obj(JSONHANDLE* in_handle, const char* find_key);
//recursevily iterate through json object in_handle and call the proc_func, where user can do something with that object
//the object is given to the user as found_handle_obj variable
int app_json_iterate_objs_run_callback(JSONHANDLE* in_handle,
				       const char* json_name, const char* json_parent, const char* top_name,
				       void* arg,
				       void(*proc_func)(void*, const char* js_name, const char* parent_name,
							const char* top_node_name, JSONHANDLE* found_handle_obj));
//create an empty json object
int app_json_create_obj(JSONHANDLE** obj);
//given file path tokenise the contents of the file and return a json_object
JSONHANDLE* app_json_tokenise_path(const char* file_path);
//free the object memory
int app_json_clean_object(JSONHANDLE* in_handle);
