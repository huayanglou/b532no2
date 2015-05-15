#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include "job.h"
//#define DEBUG

int jobid=0;
int siginfo=1;
int fifo;
int globalfd;

struct waitqueue *head1=NULL, *head2=NULL, *head3=NULL;
struct waitqueue *next=NULL,*current =NULL;

/* ���ȳ��� */
void scheduler()
{
    int clockA=0, clockB=0;
	struct jobinfo *newjob=NULL;
	struct jobcmd cmd;
	int  count = 0;
	bzero(&cmd,DATALEN);
	if((count=read(fifo,&cmd,DATALEN))<0)
		error_sys("read fifo failed");
#ifdef DEBUG
    printf("READING WHETHER OTHER PROCESS SEND COMMAND!\n");
	if(count){
		printf("cmd cmdtype\t%d\ncmd defpri\t%d\ncmd data\t%s\n",cmd.type,cmd.defpri,cmd.data);
	}
	else
		printf("no data read\n");
#endif

	/* ���µȴ������е���ҵ */
	#ifdef DEBUG
        printf("UPDATE JOBS IN WAIT QUEUE!\n");
    #endif // DEBUG
	updateall();

	switch(cmd.type){
	case ENQ:
        #ifdef DEBUG
            printf("EXECUTE ENQ!\n");
        #endif // DEBUG
		do_enq(newjob,cmd);
		#ifdef DEBUG
            printf("EXECUTED ENQ!\n");
        #endif // DEBUG
		break;
	case DEQ:
        #ifdef DEBUG
            printf("EXECUTE DEQ!\n");
        #endif // DEBUG
		do_deq(cmd);
		break;
	case STAT:
        #ifdef DEBUG
            printf("EXECUTE STAT!\n");
        #endif // DEBUG
		do_stat(cmd);
		break;
	default:
		break;
	}

    if (head3)
    {
        #ifdef DEBUG
            printf("333333SELECT WHICH JOB TO RUN NEXT!\n");
        #endif // DEBUG
        next=jobselect();
        #ifdef DEBUG
            printf("3333333SWITCH TO NEXT JOB!\n");
        #endif // DEBUG
        jobswitch();
    }else if (head2)
    {
        clockA++;
        if (clockA==2)
        {
            clockA=0;
            #ifdef DEBUG
            printf("22222SELECT WHICH JOB TO RUN NEXT!\n");
            #endif // DEBUG
            next=jobselect();
            #ifdef DEBUG
                printf("222222SWITCH TO NEXT JOB!\n");
            #endif // DEBUG
            jobswitch();
        }
    }else if (head1)
    {
        clockB++;
        #ifdef DEBUG
            printf("clockB==%d\n",clockB);
        #endif // DEBUG
        if (clockB==5)
        {
            clockB=0;
            #ifdef DEBUG
            printf("11111SELECT WHICH JOB TO RUN NEXT!!!!!!!\n");
            #endif // DEBUG
            next=jobselect();
            #ifdef DEBUG
                printf("111111SWITCH TO NEXT JOB!\n");
            #endif // DEBUG
            jobswitch();
        }
    }else
        ;


	/* ѡ������ȼ���ҵ */
	//next=jobselect();
	/* ��ҵ�л� */
	//jobswitch();
}

int allocjid()
{
	return ++jobid;
}

void updateall()
{
	struct waitqueue *p,*prep,*q;

	/* ������ҵ����ʱ�� */
	if(current)
		current->job->run_time += 1; /* ��1����1000ms */

	/* ������ҵ�ȴ�ʱ�估���ȼ� */
	/*for(p = head; p != NULL; p = p->next){
		p->job->wait_time += 1000;
		if(p->job->wait_time >= 5000 && p->job->curpri < 3){
			p->job->curpri++;
			p->job->wait_time = 0;
		}
	}*/
	for (p=head3;p!=NULL;p=p->next)
	{
        p->job->wait_time+=1000;
	}

    for (p=prep=head2;p!=NULL;prep=p,p=p->next)
    {
        p->job->wait_time+=1000;
        if(p->job->wait_time >= 5000 && p->job->curpri < 3){
			p->job->curpri++;
			p->job->wait_time = 0;
		}
		if (p->job->curpri==3)
		{
            //���ȼ�����������µ���ҵ���ж�β��
            if (head3)
            {
                for (q=head3;q->next!=NULL;q=q->next);
                q->next=p;
                prep->next=p->next;
            }
            else
            {
                head3=p;
                prep->next=p->next;
            }

		}
    }

    for (p=prep=head1;p!=NULL;prep=p,p=p->next)
    {
        p->job->wait_time+=1000;
        if(p->job->wait_time >= 10000 && p->job->curpri < 3){
			p->job->curpri++;
			p->job->wait_time = 0;
		}
		if (p->job->curpri==2)
		{
            //���ȼ�����������µ���ҵ���ж�β��
            if (head2)
            {
                for (q=head2;q->next!=NULL;q=q->next);
                q->next=p;
                prep->next=p->next;
            }
            else
            {
                head2=p;
                prep->next=p->next;
            }

		}
    }



}

struct waitqueue* jobselect()
{
	struct waitqueue *p,*select;
	//int highest = -1;

	select = NULL;
	//selectprev = NULL;
	/*if(head){
		/* �����ȴ������е���ҵ���ҵ����ȼ���ߵ���ҵ
		for(prev = head, p = head; p != NULL; prev = p,p = p->next)
			if(p->job->curpri > highest){
				select = p;
				selectprev = prev;
				highest = p->job->curpri;
			}
			selectprev->next = select->next;
			if (select == selectprev)
				head = NULL;
	}*/

	if (head3)
	{
        p=head3;
        select=p;
        if (p->next=NULL)
            head3=NULL;
        else
            head3=p->next;
	}else if (head2)
	{
        p=head2;
        select=p;
        if (p->next=NULL)
            head2=NULL;
        else
            head2=p->next;
	}else if (head1)
	{
        p=head1;
        select=p;
        if (p->next=NULL)
            head1=NULL;
        else
            head1=p->next;
	}
	return select;
}

void jobswitch()
{
	struct waitqueue *p,*q;
	int i;

	if(current && current->job->state == DONE){ /* ��ǰ��ҵ��� */
		/* ��ҵ��ɣ�ɾ���� */
		for(i = 0;(current->job->cmdarg)[i] != NULL; i++){
			free((current->job->cmdarg)[i]);
			(current->job->cmdarg)[i] = NULL;
		}
		/* �ͷſռ� */
		free(current->job->cmdarg);
		free(current->job);
		free(current);

		current = NULL;
	}

	if(next == NULL && current == NULL) /* û����ҵҪ���� */
		return;
	else if (next != NULL && current == NULL){ /* ��ʼ�µ���ҵ */

		printf("begin start new job\n");
		current = next;
		next = NULL;
		current->job->state = RUNNING;
		kill(current->job->pid,SIGCONT);
		return;
	}
	else if (next != NULL && current != NULL){ /* �л���ҵ */

		printf("switch to Pid: %d\n",next->job->pid);
		kill(current->job->pid,SIGSTOP);
		current->job->curpri = current->job->defpri;
		current->job->wait_time = 0;
		current->job->state = READY;

		/* �Żصȴ����� */
		/*if(head){
			for(p = head; p->next != NULL; p = p->next);
			p->next = current;
		}else{
			head = current;
		}*/

		if (current->job->defpri==1)
        {
            if (head1)
            {
                for(p=head1;p->next != NULL; p=p->next);
                p->next=current;
            }else
                head1=current;
        }else if (current->job->defpri==2)
        {
            if (head2)
            {
                for(p=head2;p->next != NULL; p=p->next);
                p->next=current;
            }else
                head2=current;
        }else if (current->job->defpri==3)
        {
            if (head3)
            {
                for(p=head3;p->next != NULL; p=p->next);
                p->next=current;
            }else
                head3=current;
        }

		current = next;
		next = NULL;
		current->job->state = RUNNING;
		current->job->wait_time = 0;
		kill(current->job->pid,SIGCONT);
		return;
	}else /* next == NULL��current != NULL�����л� */
        /*if(current->job->wait_time >= 10000 && p->job->curpri < 3){
			p->job->curpri++;
			p->job->wait_time = 0;
		}
		if (p->job->curpri==2)
		{
            //���ȼ�����������µ���ҵ���ж�β��
            if (head2)
            {
                for (q=head2;q->next!=NULL;q=q->next);
                q->next=p;
                prep->next=p->next;
            }
            else
            {
                head2=p;
                prep->next=p->next;
            }

		}*/


		/*if (current->job->curpri>1)
		{
            printf("priority down: job pid-%d\n",current->job->pid);
            kill(current->job->pid,SIGSTOP);
            current->job->curpri--;
            current->job->state = READY;
            if (current->job->curpri==1)
            {
                if (head1)
                {
                    for (q=head1;q->next!=NULL;q=q->next);
                    q->next=current;
                }else
                    head1=current;
            }else if (current->job->curpri==2)
            {
                if (head2)
                {
                    for (q=head2;q->next!=NULL;q=q->next);
                    q->next=current;
                }else
                    head2=current;
            }
		}*/

		return;

}

void sig_handler(int sig,siginfo_t *info,void *notused)
{
	int status;
	int ret;

	switch (sig) {
case SIGVTALRM: /* �����ʱ�������õļ�ʱ��� */
	scheduler();
	#ifdef DEBUG
        printf("SIGVTALRM RECEIVED!\n");
	#endif // DEBUG
	return;
case SIGCHLD: /* �ӽ��̽���ʱ���͸������̵��ź� */
	ret = waitpid(-1,&status,WNOHANG);
	if (ret == 0)
		return;
	if(WIFEXITED(status)){
		current->job->state = DONE;
		printf("normal termation, exit status = %d\n",WEXITSTATUS(status));
	}else if (WIFSIGNALED(status)){
		printf("abnormal termation, signal number = %d\n",WTERMSIG(status));
	}else if (WIFSTOPPED(status)){
		printf("child stopped, signal number = %d\n",WSTOPSIG(status));
	}
	return;
	default:
		return;
	}
}

void do_enq(struct jobinfo *newjob,struct jobcmd enqcmd)
{
	struct waitqueue *newnode,*p;
	int i=0,pid;
	char *offset,*argvec,*q;
	char **arglist;
	sigset_t zeromask;

	sigemptyset(&zeromask);

	/* ��װjobinfo���ݽṹ */
	newjob = (struct jobinfo *)malloc(sizeof(struct jobinfo));
	newjob->jid = allocjid();
	newjob->defpri = enqcmd.defpri;
	newjob->curpri = enqcmd.defpri;
	newjob->ownerid = enqcmd.owner;
	newjob->state = READY;
	newjob->create_time = time(NULL);
	newjob->wait_time = 0;
	newjob->run_time = 0;
	arglist = (char**)malloc(sizeof(char*)*(enqcmd.argnum+1));
	newjob->cmdarg = arglist;
	offset = enqcmd.data;
	argvec = enqcmd.data;
	while (i < enqcmd.argnum){
		if(*offset == ':'){
			*offset++ = '\0';
			q = (char*)malloc(offset - argvec);
			strcpy(q,argvec);
			arglist[i++] = q;
			argvec = offset;
		}else
			offset++;
	}

	arglist[i] = NULL;

//#ifdef DEBUG

	printf("enqcmd argnum %d\n",enqcmd.argnum);
	for(i = 0;i < enqcmd.argnum; i++)
		printf("parse enqcmd:%s\n",arglist[i]);

//#endif

	/*��ȴ������������µ���ҵ*/
	newnode = (struct waitqueue*)malloc(sizeof(struct waitqueue));
	newnode->next =NULL;
	newnode->job=newjob;

	/*if(head)
	{
		for(p=head;p->next != NULL; p=p->next);
		p->next =newnode;
	}else
		head=newnode;
    */

    if (newjob->defpri==1)
    {
        if (head1)
        {
            for(p=head1;p->next != NULL; p=p->next);
            p->next=newnode;
        }else
            head1=newnode;
    }else if (newjob->defpri==2)
    {
        if (head2)
        {
            for(p=head2;p->next != NULL; p=p->next);
            p->next=newnode;
        }else
            head2=newnode;
    }else if (newjob->defpri==3)
    {
        if (head3)
        {
            for(p=head3;p->next != NULL; p=p->next);
            p->next=newnode;
        }else
            head3=newnode;
    }



	/*Ϊ��ҵ��������*/
	if((pid=fork())<0)
		error_sys("enq fork failed");

	if(pid==0){
		newjob->pid =getpid();
		/*�����ӽ���,�ȵ�ִ��*/
		raise(SIGSTOP);
#ifdef DEBUG

		printf("begin running\n");
		for(i=0;arglist[i]!=NULL;i++)
			printf("arglist %s\n",arglist[i]);
#endif

		/*�����ļ�����������׼���*/
		dup2(globalfd,1);
		/* ִ������ */
		if(execv(arglist[0],arglist)<0)
			printf("exec failed\n");
		exit(1);
	}else{
		newjob->pid=pid;
		wait(NULL);
	}
}

void do_deq(struct jobcmd deqcmd)
{
	int deqid,i;
	struct waitqueue *p,*prev,*select,*selectprev;
	deqid=atoi(deqcmd.data);

#ifdef DEBUG
	printf("deq jid %d\n",deqid);
#endif

	/*current jodid==deqid,��ֹ��ǰ��ҵ*/
	if (current && current->job->jid ==deqid){
		printf("teminate current job\n");
		kill(current->job->pid,SIGKILL);
		for(i=0;(current->job->cmdarg)[i]!=NULL;i++){
			free((current->job->cmdarg)[i]);
			(current->job->cmdarg)[i]=NULL;
		}
		free(current->job->cmdarg);
		free(current->job);
		free(current);
		current=NULL;
	}
	else{ /* �����ڵȴ������в���deqid */
		select=NULL;
		selectprev=NULL;
		/*if(head){
			for(prev=head,p=head;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				selectprev->next=select->next;
				if(select==selectprev)
					head=NULL;
		}*/

		if(head1){
			for(prev=head1,p=head1;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				selectprev->next=select->next;
				if(select==selectprev)
					head1=NULL;
		}

		if(head2){
			for(prev=head2,p=head2;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				selectprev->next=select->next;
				if(select==selectprev)
					head2=NULL;
		}

		if(head3){
			for(prev=head3,p=head3;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				selectprev->next=select->next;
				if(select==selectprev)
					head3=NULL;
		}

		if(select){
			for(i=0;(select->job->cmdarg)[i]!=NULL;i++){
				free((select->job->cmdarg)[i]);
				(select->job->cmdarg)[i]=NULL;
			}
			free(select->job->cmdarg);
			free(select->job);
			free(select);
			select=NULL;
		}
	}
}

void do_stat(struct jobcmd statcmd)
{
	struct waitqueue *p;
	char timebuf[BUFLEN];
	/*
	*��ӡ������ҵ��ͳ����Ϣ:
	*1.��ҵID
	*2.����ID
	*3.��ҵ������
	*4.��ҵ����ʱ��
	*5.��ҵ�ȴ�ʱ��
	*6.��ҵ����ʱ��
	*7.��ҵ״̬
	*/

	/* ��ӡ��Ϣͷ�� */
	printf("QUEUE\tJOBID\tPID\tOWNER\tRUNTIME\tWAITTIME\tCREATTIME\t\tSTATE\n");
	if(current){
		strcpy(timebuf,ctime(&(current->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("CURRENT\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			current->job->jid,
			current->job->pid,
			current->job->ownerid,
			current->job->run_time,
			current->job->wait_time,
			timebuf,"RUNNING");
	}

	/*for(p=head;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}*/

	for(p=head3;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("Q3\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}

	for(p=head2;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("Q2\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}

	for(p=head1;p!=NULL;p=p->next){
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		printf("Q1\t%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}
}

int main()
{
	struct timeval interval;
	struct itimerval new,old;
	struct stat statbuf;
	struct sigaction newact,oldact1,oldact2;

	#ifdef DEBUG
        printf("DEBUG IS OPEN!\n");
    #endif

	if(stat("/tmp/server",&statbuf)==0){
		/* ���FIFO�ļ�����,ɾ�� */
		if(remove("/tmp/server")<0)
			error_sys("remove failed");
	}

	if(mkfifo("/tmp/server",0666)<0)
		error_sys("mkfifo failed");
	/* �ڷ�����ģʽ�´�FIFO */
	if((fifo=open("/tmp/server",O_RDONLY|O_NONBLOCK))<0)
		error_sys("open fifo failed");

	/* �����źŴ����� */
	newact.sa_sigaction=sig_handler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags=SA_SIGINFO;
	sigaction(SIGCHLD,&newact,&oldact1);
	sigaction(SIGVTALRM,&newact,&oldact2);

	/* ����ʱ����Ϊ1000���� */
	interval.tv_sec=1;
	interval.tv_usec=0;

	new.it_interval=interval;
	new.it_value=interval;
	setitimer(ITIMER_VIRTUAL,&new,&old);

	while(siginfo==1);

	close(fifo);
	close(globalfd);
	return 0;
}
