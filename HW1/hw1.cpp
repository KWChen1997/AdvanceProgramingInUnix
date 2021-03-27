#define boolString(x) (x?"true":"false")

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <fstream>

// getopt libs
#include <unistd.h>

// directory libs
#include <sys/types.h>
#include <dirent.h>

// stat
#include <sys/stat.h>

// regex
#include <regex>

// getpwuid
#include <pwd.h>

// errno
#include <errno.h>

#include <unordered_set>

using namespace std;

class ProcData{
public:
	char cmd[256];
	char pid[6];
	char usr[256];
	char *path;
};

static const char format[] = "%-36s%5s%18s%5s%10s%10lu %s\n";
vector<string> output;

void getProcData(const string pid);
void getCmdAndUsrname(const char *cwd, char *command, char *username);
void getInfo(ProcData *proc, const char *fd);
void getInfo(ProcData *proc);
void checkType(mode_t t, char *type);
void getMaps(ProcData *proc);

int main(int argc, char *argv[]){
	const char *cwd = get_current_dir_name();
	//printf("process directory: %s\n",cwd);
	/*  parse the arguement input
	 *
	 *  -c: command line regex search
	 *  -t: type filter
	 *  -f: file name filter
	 *
	 *  todo: parse the filter arguements
	 *  */
	int opt;
	bool CLIFlag = 0;
	bool typeFlag = 0;
	bool fileFlag = 0;
	char CLIFilter[1024] = {0};
	char typeFilter[1024] = {0};
	char fileFilter[1024] = {0};
	regex eTypeFilter("(REG|CHR|DIR|FIFO|SOCK)");
	
	while((opt = getopt(argc, argv, "ctf")) != -1){
		switch(opt){
			case 'c':
				CLIFlag = true;
				strcpy(CLIFilter,argv[optind]);
				break;
			case 't':
				typeFlag = true;
				strcpy(typeFilter,argv[optind]);
				if(!regex_match(typeFilter,eTypeFilter)){
					printf("Invalid TYPE option\n");
					return -1;
				}
				break;
			case 'f':
				fileFlag = true;
				strcpy(fileFilter,argv[optind]);
				break;
			default:
				printf("option \"%c\" not specified\n",(char)opt);
				break;
		}
	}
	
	//printf("CLIFlag: %s\nfilter: %s\ntypeFlag: %s\nfilter: %s\nfileFlag: %s\nfilter: %s\n", boolString(CLIFlag),CLIFilter, boolString(typeFlag),typeFilter, boolString(fileFlag), fileFilter);

	/*
	 * read directory of /proc/
	 *
	 * */

	chdir("/proc/");
	int count = 0;
	DIR *dp;
	struct dirent *dirp;
	dp = opendir("/proc");
	printf("%-36s%5s%18s%5s%10s%13s %s\n","COMMAND","PID","USER","FD","TYPE","NODE","NAME");
	if(dp == NULL){
		printf("failed to open directory\n");
		return 1;
	}
	else{
		regex reg("[0-9]+");
		count = 0;
		while((dirp = readdir(dp)) != NULL && count < 10){
			if(regex_match(dirp->d_name,reg)){
				//printf("%s:\n", dirp->d_name);
				getProcData(dirp->d_name);
				//count++;
			}
		}
		closedir(dp);
	}

	chdir(cwd);
	
	// print out results
	char cmd[256];
	char type[256];
	char filename[256];
	bool valid;
	for(auto line:output){
		valid = true;
		memset(cmd,0,sizeof(cmd));
		memset(type,0,sizeof(type));
		memset(filename,0,sizeof(filename));
		regex eCLIFilter(CLIFilter);
		regex eTypeFilter(typeFilter);
		regex eFileFilter(fileFilter);

		sscanf(&line[0],"%s %*s %*s %*s %s %*s %s",cmd,type,filename);

		valid = (!CLIFlag || regex_match(cmd,eCLIFilter)) &&
			(!typeFlag || regex_match(type,eTypeFilter)) &&
			(!fileFlag || regex_match(filename,eFileFilter));
		if(valid)
			printf("%s",&line[0]);
	}

	return 0;
}

void getProcData(const string pid){
	chdir(&pid[0]);
	ProcData proc;

	proc.path = get_current_dir_name();
	strcpy(proc.pid, &pid[0]);

	getCmdAndUsrname(proc.path, proc.cmd, proc.usr);
	

	// get cwd info
	//sprintf(path,"%s%s%s","/proc/",&pid[0],"/cwd");
	getInfo(&proc,"cwd");

	// get root info
	getInfo(&proc,"root");

	// get exe info
	getInfo(&proc,"exe");

	// get mem info
	getMaps(&proc);

	// get fd info
	getInfo(&proc);

	// get del info	

	chdir("../");

}

void getInfo(ProcData *proc, const char *fd){

	char path[1024];
	char type[10];
	char result[1024];
	memset(result,0,sizeof(result));

	sprintf(path,"%s/%s",proc->path, fd);
	int c;

	char buf[1024];
	c = readlink(path, buf, sizeof(buf));
	if(c == -1){
		if(errno == EACCES){
			sprintf(result,"%-36s%5s%18s%5s%10s%13s %s%s", proc->cmd, proc->pid, proc->usr, fd,"unknown", "", path, " (readlink: Permission denied)\n");
		}
		else{
			sprintf(result,"%-36s%5s%18s%5s%10s%13s\n", proc->cmd, proc->pid, proc->usr, fd,"unknown", "");
		}
	}
	else{
		buf[c] = '\0';
		struct stat fs;
		c = stat(buf,&fs);
		checkType(fs.st_mode, type);
		sprintf(result,format, proc->cmd, proc->pid, proc->usr, fd, type, fs.st_ino,buf);
	}

	output.push_back(string(result));
	return;
}

void checkType(mode_t t, char *type){
	if(S_ISDIR(t)){
		strcpy(type,"DIR");
	}
	else if(S_ISREG(t)){
		strcpy(type,"REG");
	}
	else if(S_ISCHR(t)){
		strcpy(type,"CHR");
	}
	else if(S_ISFIFO(t)){
		strcpy(type,"FIFO");
	}
	else if(S_ISSOCK(t)){
		strcpy(type,"SOCK");
	}
	else{
		strcpy(type,"unknown");
	}
	return;
}

void getInfo(ProcData *proc){
	char path[1024];
	char type[10];
	char result[1024];
	cmatch match;
	memset(path,0,sizeof(path));
	memset(result,0,sizeof(result));
	sprintf(path,"%s/%s",proc->path, "fd");
	int c;
	long int inode;
	regex eFile("[0-9]+");
	regex eType("^([^:]+):(.*)$");
	regex eFD("(\\d{2})$");
	char fullType[256];
	char name[256];
	char fdbytes[3];
	char fdFlag;

	char buf[1024];
	c = access(path, R_OK);
	DIR *dp;
	struct dirent *dirp;

	dp = opendir(path);

	if(dp == NULL && errno == EACCES){
		sprintf(result,"%-36s%5s%18s%5s%10s%13s %s %s\n",proc->cmd, proc->pid, proc->usr, "NOFD","","",path,"(opendir: Permission denied)");
		output.push_back(string(result));
	}
	else{
		while((dirp = readdir(dp)) != NULL){
			inode = 0;
			memset(type,0,sizeof(type));
			memset(fullType,0,sizeof(fullType));
			memset(name,0,sizeof(name));
			if(regex_match(dirp->d_name,eFile)){
				sprintf(path,"%s/fd/%s",proc->path,dirp->d_name);
				c = readlink(path,buf,sizeof(buf));
				buf[c] = '\0';
								struct stat fs;
				c = stat(buf,&fs);
				if(c < 0){
					regex_search(buf,match,eType);
					memcpy(fullType,match[1].first,match[1].second - match[1].first);
					memcpy(name,match[2].first,match[2].second - match[2].first);
					if(strncmp(fullType,"pipe",4) == 0){
						strcpy(type,"FIFO");
						inode = atoi(name+1);
						strncpy(name,buf,256);
					}
					else if(strncmp(fullType,"socket",6) == 0){
						strcpy(type,"SOCK");
						inode = atoi(name+1);
						strncpy(name,buf,256);
					}
					else{
						inode = 12123lu;
						strcpy(type,"unknown");
					}
					
				}
				else{
					strncpy(name,buf,256);
					checkType(fs.st_mode,type);
					inode = fs.st_ino;
				}

				// get fd flags
				sprintf(path,"%s/fdinfo/%s",proc->path,dirp->d_name);
				ifstream ifs(path,ifstream::in);
				ifs.getline(buf,1024,'\n');
				ifs.getline(buf,1024,'\n');
				ifs.close();
				
				regex_search(buf,match,eFD);
				memcpy(fdbytes,match[1].first,match[1].second - match[1].first);

				if(strncmp(fdbytes,"00",2) == 0){
					fdFlag = 'r';
				}
				else if(strncmp(fdbytes,"01",2) == 0){
					fdFlag = 'w';
				}
				else if(strncmp(fdbytes,"02",2) == 0){
					fdFlag = 'u';
				}
				sprintf(result,"%-36s%5s%18s%5s%c%9s%13lu %s\n",proc->cmd, proc->pid, proc->usr,dirp->d_name,fdFlag,type,inode,name);
				output.push_back(string(result));
			}
		}
	}

	return;
}

void getMaps(ProcData *proc){
	char path[1024];
	char type[10];
	char result[1024];
	bool deleted = false;
	regex eDel(".*\\(deleted\\)$"); 
	memset(path,0,sizeof(path));
	memset(result,0,sizeof(result));
	sprintf(path,"%s/%s",proc->path, "maps");

	if(access(path, R_OK) < 0){
		return;
	}

	char buf[1024];
	ifstream ifs(path,ifstream::in);

	unordered_set<string> m;
	char memName[256];
	ino_t inode;
	while(ifs.getline(buf,1024,'\n')){
		sscanf(buf,"%*s %*s %*s %lu %*s",&inode);
		if(inode == 0)
			break;
	}

	while(ifs.getline(buf,1024,'\n')){
		memset(memName,0,sizeof(memName));
		deleted = false;
		sscanf(buf,"%*s %*s %*s %*s %lu %s", &inode, memName);
		if(regex_match(buf,eDel)){
			deleted = true;
		}
		string tmp(memName);
		if(inode == 0lu || m.find(tmp) != m.end()){
			continue;
		}
		m.insert(tmp);
		struct stat fs;
		stat(memName,&fs);
		checkType(fs.st_mode,type);
		if(deleted){
			sprintf(result,"%-36s%5s%18s%5s%10s%10lu %s (deleted)\n",proc->cmd, proc->pid, proc->usr, "DEL",type,inode,memName);
		}
		else{
			sprintf(result,"%-36s%5s%18s%5s%10s%10lu %s\n",proc->cmd, proc->pid, proc->usr, "mem",type,inode,memName);
		}
		output.push_back(string(result));
	}
	return;
}

void getCmdAndUsrname(const char *cwd, char *command, char *username){
	char path[1024] = {0};
	char buffer[1024] = {0};

	sprintf(path,"%s%s", cwd, "/status");
	ifstream ifs(path, ifstream::in);

	// get command field
	ifs.getline(buffer, 256, '\n');
	strcpy(command, buffer + 6);

	
	// get username field
	regex e("^Uid.*");
	while(ifs.getline(buffer,256,'\n')){
		if(regex_match(buffer,e)){
			break;
		}
	}
	ifs.close();

	struct passwd *user;
	uid_t uid;
	sscanf(buffer,"%*s %d", &uid);
	user = getpwuid(uid);

	strcpy(username, user->pw_name);

	return;
}
