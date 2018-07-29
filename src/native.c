#include "main.h"

duk_ret_t native_puts(duk_context *ctx) { //Just prints lmao
	if(duk_get_top(ctx) == 0) return 0;
	puts(duk_to_string(ctx, 0));
	return 0;
}

duk_ret_t native_listdir(duk_context *ctx) { //lists directory as array
	if(duk_get_top(ctx) == 0) return 0;
	const char *path = resolve_path(duk_to_string(ctx, 0));
	
	duk_idx_t return_array = duk_push_array(ctx);
	duk_uarridx_t array_index = 0;
	
	struct dirent *dir;
	DIR *d = opendir(path);
	if(d) {
		while( (dir = readdir(d)) != NULL ) {
			if( strcmp(".", dir->d_name) == 0 || strcmp("..", dir->d_name) == 0) continue; //ignore "." & ".."
			duk_push_string(ctx, dir->d_name);
			duk_put_prop_index(ctx, return_array, array_index++);
		}
		closedir(d);
	} else {
		return 0; //if folder doesn't exist
	}
	return 1;
}

duk_ret_t native_exists(duk_context *ctx) { //returns either "file" or "directory", fails with undefined
	if(duk_get_top(ctx) == 0) return 0;
	
	struct stat properties;
    if(stat(resolve_path(duk_to_string(ctx, 0)), &properties) == 0) {
		if(S_ISDIR(properties.st_mode)) {
			duk_push_string(ctx, "directory");
		} else if(S_ISREG(properties.st_mode)) {
			duk_push_string(ctx, "file");
		}
		return 1;
	}
	
	return 0;
}

duk_ret_t native_readfile(duk_context *ctx) { //read file and return the contents as a JS String
	if(duk_get_top(ctx) == 0) return 0;
	FILE *f = fopen(resolve_path(duk_to_string(ctx, 0)), "r");
	if(f == NULL) return 0;
	char buffer[1024]; //read in 1k chunks
	size_t chunks = 0;
	size_t received_bytes = 0;
	duk_push_string(ctx, ""); //delimiter
	while(received_bytes = fread(buffer, sizeof(char), sizeof(buffer), f)) {
		duk_push_lstring(ctx, buffer, received_bytes);
		chunks++;
	}
	duk_join(ctx, chunks);
	return 1;
}

duk_ret_t native_writefile(duk_context *ctx) { //return undefined if syntax incorrect, return false if unable to write, true if successful
	if(duk_get_top(ctx) < 2) return 0; //2 args, first is filename, second is content as string
	FILE *f = fopen(resolve_path(duk_to_string(ctx, 0)), "w");
	if(f == NULL) {
		duk_push_boolean(ctx, 0);
		return 1;
	}
	
	duk_size_t contents_length;
	const char *contents = duk_get_lstring(ctx, 1, &contents_length); //This will be what we write to file
	fwrite(contents, sizeof(char), contents_length, f); //-1 for \0
	fclose(f);
	
	duk_push_boolean(ctx, 1);
	return 1;
}

duk_ret_t native_removefile(duk_context *ctx) { //delete file
	if(duk_get_top(ctx) == 0) return 0;
	
	if(remove(resolve_path(duk_to_string(ctx, 0))) != 0) {
		duk_push_boolean(ctx, 0);
		return 1;
	}
	
	duk_push_boolean(ctx, 1);
	return 1;
}

duk_ret_t native_runfile(duk_context *ctx) { //read and eval file
	if(duk_get_top(ctx) == 0) return 0;
	FILE *f = fopen(resolve_path(duk_to_string(ctx, 0)), "r");
	if(f == NULL) return 0;
	char buffer[1024]; //read in 1k chunks
	size_t chunks = 0;
	size_t received_bytes = 0;
	duk_push_string(ctx, ""); //delimiter
	while(received_bytes = fread(buffer, sizeof(char), sizeof(buffer), f)) {
		duk_push_lstring(ctx, buffer, received_bytes);
		chunks++;
	}
	duk_join(ctx, chunks);
	duk_peval(ctx);
	return 1;
}

duk_ret_t native_sandbox(duk_context *ctx) { //like eval but won't kill program on error
	if(duk_get_top(ctx) == 0) return 0;
	duk_peval(ctx);
	return 1;
}

duk_ret_t native_stdin(duk_context *ctx) { //get user input
	char buffer[1024];
	if(duk_get_top(ctx) > 0) {
		printf("%s", duk_to_string(ctx, 0));
		fflush(stdout); //Just in case the output doesn't get flush (like in some IDEs / Terminals)
	}
	
	char *check = fgets(buffer, sizeof(buffer), stdin);
	size_t check_len = strlen(buffer)-1;
	
	if(check) {
		duk_push_lstring(ctx, buffer, check_len); //remove \n from fgets
		return 1;
	}
	return 0;
}

duk_ret_t native_mkdir(duk_context *ctx) { //create directory
	if(duk_get_top(ctx) == 0) return 0;
	
	if(mkdir(resolve_path(duk_to_string(ctx, 0)), 755) != 0) {
		duk_push_boolean(ctx, 0);
		return 1;
	}
	
	duk_push_boolean(ctx, 1);
	return 1;
}

duk_ret_t native_rmdir(duk_context *ctx) { //remove empty directory
	if(duk_get_top(ctx) == 0) return 0;
	
	if(rmdir(resolve_path(duk_to_string(ctx, 0))) != 0) {
		duk_push_boolean(ctx, 0);
		return 1;
	}
	
	duk_push_boolean(ctx, 1);
	return 1;
}

duk_ret_t native_chdir(duk_context *ctx) { //change directory
	if(duk_get_top(ctx) > 0) {
		duk_size_t path_length;
		const char *str = duk_get_lstring(ctx, 0, &path_length);
		char *buffer = malloc(path_length +1); //+1 for '/'
		memcpy(buffer, str, path_length);
		*(buffer + path_length) = '/';
		*(buffer + path_length +1) = '\0';
		
		char CURRENT_PATH_BACKUP[CURRENT_PATH_SIZE+1];
		size_t CURRENT_PATH_BACKUP_SIZE = CURRENT_PATH_SIZE+1;
		memcpy(CURRENT_PATH_BACKUP, CURRENT_PATH, CURRENT_PATH_BACKUP_SIZE);
		
		const char *path = resolve_path(buffer);
		free(buffer);
		
		if(chdir(path) == 0) {
			duk_push_string(ctx, resolve_path_to_js(path));
			return 1;
		} else { //if it failed
			memcpy(CURRENT_PATH, CURRENT_PATH_BACKUP, CURRENT_PATH_BACKUP_SIZE);
			CURRENT_PATH_SIZE = CURRENT_PATH_BACKUP_SIZE-1;
		}
	}
	return 0;
}