#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <query_classifier.h>
#include <buffer.h>
#include <mysql.h>
#include <unistd.h>

static char* server_options[] = {
    "SkySQL Gateway",
    "--no-defaults",
    "--datadir=.",
    "--language=.",
    "--skip-innodb",
    "--default-storage-engine=myisam",
	NULL
};

const int num_elements = (sizeof(server_options) / sizeof(char *)) - 1;

static char* server_groups[] = {
	"embedded",
	"server",
	"server",
	NULL
};

int main(int argc, char** argv)
{
	if(argc < 3){
		fprintf(stderr,"Usage: classify <input> <expected output>");
		return 1;
	}
	int rd = 0,buffsz = getpagesize(),strsz = 0,ex_val = 0;
	char buffer[buffsz], *strbuff = (char*)calloc(buffsz,sizeof(char));
	FILE *input,*expected;

	if(mysql_library_init(num_elements, server_options, server_groups))
		{
			printf("Error: Cannot initialize Embedded Library.");
		    return 1;
		}

	input = fopen(argv[1],"rb");
	expected = fopen(argv[2],"rb");

	while((rd = fread(buffer,sizeof(char),buffsz,input))){
		
		/**Fill the read buffer*/
		if(strsz + rd >= buffsz){
			char* tmp = (char*)calloc((buffsz*2),sizeof(char));
			
			if(!tmp){
				fprintf(stderr,"Error: Cannot allocate enough memory.");
				return 1;
			}
			memcpy(tmp,strbuff,buffsz);
			free(strbuff);
			strbuff = tmp;
			buffsz *= 2;
		}
		
		memcpy(strbuff+strsz,buffer,rd);
		strsz += rd;

		char *tok,*nlptr;

		/**Remove newlines*/
		while((nlptr = strpbrk(strbuff,"\n")) != NULL && (nlptr - strbuff) < strsz){
			memmove(nlptr,nlptr+1,strsz - (nlptr + 1 - strbuff));
			strsz -= 1;
		}


		/**Parse read buffer for full queries*/

		while(strpbrk(strbuff,";") != NULL){
			tok = strpbrk(strbuff,";");
			unsigned int qlen = tok - strbuff + 1;
			GWBUF* buff = gwbuf_alloc(qlen+6);
			*((unsigned char*)(buff->start)) = qlen;
			*((unsigned char*)(buff->start + 1)) = (qlen >> 8);
			*((unsigned char*)(buff->start + 2)) = (qlen >> 16);
			*((unsigned char*)(buff->start + 3)) = 0x00;
			*((unsigned char*)(buff->start + 4)) = 0x03;
			memcpy(buff->start+5, strbuff, qlen);
			memmove(strbuff,tok + 1, strsz - qlen);
			strsz -= qlen;
			memset(strbuff + strsz,0,buffsz - strsz);
			skygw_query_type_t type = query_classifier_get_type(buff);
			char qtypestr[64];
			char expbuff[256];
			int expos = 0;
			
			while((rd = fgetc(expected)) != '\n' && !feof(expected)){
				expbuff[expos++] = rd;
			}
			expbuff[expos] = '\0';

			if(type == QUERY_TYPE_UNKNOWN){
				sprintf(qtypestr,"QUERY_TYPE_UNKNOWN");
			}
			if(type & QUERY_TYPE_LOCAL_READ){
				sprintf(qtypestr,"QUERY_TYPE_LOCAL_READ");
			}
			if(type & QUERY_TYPE_READ){
				sprintf(qtypestr,"QUERY_TYPE_READ");
			}
			if(type & QUERY_TYPE_WRITE){
				sprintf(qtypestr,"QUERY_TYPE_WRITE");
			}
			if(type & QUERY_TYPE_MASTER_READ){
				sprintf(qtypestr,"QUERY_TYPE_MASTER_READ");
			}
			if(type & QUERY_TYPE_SESSION_WRITE){
				sprintf(qtypestr,"QUERY_TYPE_SESSION_WRITE");
			}
			if(type & QUERY_TYPE_USERVAR_READ){
				sprintf(qtypestr,"QUERY_TYPE_USERVAR_READ");
			}
			if(type & QUERY_TYPE_SYSVAR_READ){
				sprintf(qtypestr,"QUERY_TYPE_SYSVAR_READ");
			}
			if(type & QUERY_TYPE_GSYSVAR_READ){
				sprintf(qtypestr,"QUERY_TYPE_GSYSVAR_READ");
			}
			if(type & QUERY_TYPE_GSYSVAR_WRITE){
				sprintf(qtypestr,"QUERY_TYPE_GSYSVAR_WRITE");
			}
			if(type & QUERY_TYPE_BEGIN_TRX){
				sprintf(qtypestr,"QUERY_TYPE_BEGIN_TRX");
			}
			if(type & QUERY_TYPE_ENABLE_AUTOCOMMIT){
				sprintf(qtypestr,"QUERY_TYPE_ENABLE_AUTOCOMMIT");
			}
			if(type & QUERY_TYPE_DISABLE_AUTOCOMMIT){
				sprintf(qtypestr,"QUERY_TYPE_DISABLE_AUTOCOMMIT");
			}
			if(type & QUERY_TYPE_ROLLBACK){
				sprintf(qtypestr,"QUERY_TYPE_ROLLBACK");
			}
			if(type & QUERY_TYPE_COMMIT){
				sprintf(qtypestr,"QUERY_TYPE_COMMIT");
			}
			if(type & QUERY_TYPE_PREPARE_NAMED_STMT){
				sprintf(qtypestr,"QUERY_TYPE_PREPARE_NAMED_STMT");
			}
			if(type & QUERY_TYPE_PREPARE_STMT){
				sprintf(qtypestr,"QUERY_TYPE_PREPARE_STMT");
			}
			if(type & QUERY_TYPE_EXEC_STMT){
				sprintf(qtypestr,"QUERY_TYPE_EXEC_STMT");
			}
			if(type & QUERY_TYPE_CREATE_TMP_TABLE){
				sprintf(qtypestr,"QUERY_TYPE_CREATE_TMP_TABLE");
			}
			if(type & QUERY_TYPE_READ_TMP_TABLE){
				sprintf(qtypestr,"QUERY_TYPE_READ_TMP_TABLE");
			}
			
			if(strcmp(qtypestr,expbuff) != 0){
				printf("Error in output: '%s' was expected but got '%s'",expbuff,qtypestr);
				ex_val = 1;
			}
			
			gwbuf_free(buff);			
		}

	}
	fclose(input);
	fclose(expected);
	free(strbuff);
	return ex_val;
}
