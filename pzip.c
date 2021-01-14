/*
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your group information here.
 *
 * Group No.: 39 (Join a project group in Canvas)
 * First member's full name: Wong Chi Ho
 * First member's email address: chwong699-c@my.cityu.edu.hk
 * Second member's full name: Leung Yiu Kwong
 * Second member's email address: ykleung64-c@my.cityu.edu.hk
 * Third member's full name:
 * Third member's email address:
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

int BUFFER_SIZE = 10485760;

int NUM_THREADS;
int numOfFile;
int handleFile_finish = 0;
int listsize = 0;
int index_cthread = 0;
int totalFileSize = 0;
int count = 0; //count how many buffer in the list
double times;
clock_t start, end;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cthreadlock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vaild = PTHREAD_COND_INITIALIZER;

struct resultSet // temp of the zipped content 
{
    char character;
    int char_count;
};

struct buffer //part of contain of text file
{
    int size;
    char *content;
    int numOfResultset;
    struct resultSet *result;
};

struct buffer *bufferlist;

void bufferlist_init(int size)// init the array of buffer
{
    bufferlist = (struct buffer *)malloc(sizeof(struct buffer) * size);
}

void *handleFile(void *arg) // divide text files for pthread to operate
{
    struct stat st;
    int size;
    char **filePath = (char **)arg;
    for (int i = 0; i < numOfFile; i++)// get the size to init the array
    {
        size = stat(filePath[i], &st);
        size = st.st_size;
	totalFileSize+=size;
        if (size / (double)BUFFER_SIZE > size / BUFFER_SIZE)
        {
            listsize += 1;
        }
        listsize += size / BUFFER_SIZE;
    }

    bufferlist_init(listsize);

    for (int i = 0; i < numOfFile; i++)// create Buffer
    {
        int fp = open(filePath[i], O_RDWR);
        char *filePointer;
        int size = stat(filePath[i], &st);
        size = st.st_size;
        filePointer = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fp, 0);
        int numOfBuffer = 0;
        if (size / (double)BUFFER_SIZE > size / BUFFER_SIZE)
        {
            numOfBuffer += 1;
        }
        numOfBuffer += size / BUFFER_SIZE;

        for (int j = 0; j < numOfBuffer; j++)
        {
            struct buffer buff;

            if (j == (numOfBuffer - 1))
            {
                buff.size = (size - (j * BUFFER_SIZE));

                buff.content = filePointer + (size - (j * BUFFER_SIZE));
            }
            else
            {
                buff.size = BUFFER_SIZE;
            }
            buff.content = filePointer + (j * BUFFER_SIZE);

            bufferlist[count] = buff;
            count++;
        }
        close(fp);
    }
    handleFile_finish = 1;
    return 0;
}

void compress(int bufferIndex)//compress text content
{
    int char_count = 0;
    char current_char = 0;
    int max = bufferlist[bufferIndex].size;
    char *textString = bufferlist[bufferIndex].content;
    struct resultSet *result = (struct resultSet *)malloc(sizeof(struct resultSet) * max);
    int r_index = 0;
    for (int i = 0; i < max; i++)//loop all char in textString
    {
        if (textString[i] == current_char)
        {
            char_count += 1;
        }
        else
        {
            if (char_count > 0)
            {
                struct resultSet rs;
                rs.char_count = char_count;
                rs.character = current_char;
                result[r_index] = rs;
                r_index += 1;
            }
            char_count = 1;
            current_char = textString[i];
        }
        
        if (i == max - 1)
            {
                struct resultSet rs;
                rs.char_count = char_count;
                rs.character = textString[i];
                result[r_index] = rs;
                r_index += 1;
            }
    }

    bufferlist[bufferIndex].numOfResultset = r_index;
    bufferlist[bufferIndex].result = result;
}

void *compressText(void *null)//call compress method to compress content of buffer one by one
{

    int do_index;
    int cthread_ready = 0;

    do
    {
        pthread_mutex_lock(&mutex);
        if (index_cthread < count)
        {
            do_index = index_cthread;
            index_cthread += 1;
            cthread_ready = 1;
        }
        pthread_mutex_unlock(&mutex);
        if (cthread_ready == 1)
        {
            compress(do_index);
            cthread_ready = 0;
        }

        if (index_cthread == listsize && handleFile_finish == 1)
        {
            return 0;
        }

    } while (1);
    return 0;
}

void output()// print out the zipped content
{
    char char_temp = 0;
    int num_temp = 0;
        char *outputString = (char *)malloc(totalFileSize * (sizeof(int) + sizeof(char)));
        char *outputString_head = outputString;
    for (int i = 0; i < listsize; i++)
    {

        for (int j = 0; j < bufferlist[i].numOfResultset; j++)
        {
            if (i > 0 && j == 0)
            {
                if (char_temp != bufferlist[i].result[j].character)
		{
			*((int*)outputString)=num_temp;
			outputString+=sizeof(int);
			*((char*)outputString)=char_temp;
			outputString+=sizeof(char);

                    num_temp = 0;
                }
            }

            if (j == bufferlist[i].numOfResultset - 1 && i != listsize - 1)
            {
                num_temp += bufferlist[i].result[j].char_count;
                char_temp = bufferlist[i].result[j].character;
                continue;
            }
            if (char_temp == bufferlist[i].result[j].character)
            {
                
			*((int*)outputString)=bufferlist[i].result[j].char_count + num_temp;
			outputString+=sizeof(int);
			*((char*)outputString)=char_temp;
			outputString+=sizeof(char);

                num_temp = 0;
            }
            else
            {
			*((int*)outputString)=bufferlist[i].result[j].char_count;
			outputString+=sizeof(int);
			*((char*)outputString)=bufferlist[i].result[j].character;
			outputString+=sizeof(char);
            }
        }
    }

    fwrite(outputString_head,outputString-outputString_head,1,stdout);
}
int main(int argc, char **argv)
{
    NUM_THREADS = get_nprocs();
    if (argc < 2)
    {
        printf("pzip: file1 [file2 ...]\n");
        exit(1);
    }
    DIR *dir;
    struct dirent *ent;

    numOfFile = argc - 1;

    char **Allpaths = (char **)malloc(sizeof(char *) * 20);
    int allpathIndex = 0;

    for (int i = 0; i < argc - 1; i++)
    {
        if ((dir = opendir(argv[i + 1])) != NULL)//get the file in dir 
        {
            numOfFile -= 1;
            while ((ent = readdir(dir)) != NULL)
            {

                if ((ent->d_type) == 8) //if text file
                {
                    char *fpath = (char *)malloc(256);
                    strcat(fpath, argv[i + 1]);
                    strcat(fpath, "/");
                    char *name = (ent->d_name);
                    strcat(fpath, name);
                    Allpaths[allpathIndex] = fpath;
                    allpathIndex += 1;
                    numOfFile += 1;
                }
            }
            closedir(dir);
        }
        else
        {
            Allpaths[allpathIndex] = argv[i + 1];
            allpathIndex += 1;
        }
    }

    pthread_t pthreads;
    pthread_create(&pthreads, NULL, handleFile, Allpaths);//file handling

    pthread_t cthreads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&cthreads[i], NULL, compressText, NULL);//compress content in buffer
    }
    pthread_join(pthreads, NULL);


    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(cthreads[i], NULL);

    }

    output();

    return 0;
}
