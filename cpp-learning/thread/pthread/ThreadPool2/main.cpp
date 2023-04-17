class CXJob:public CJob
{
public:
    CXJob(){i=0;}
    ~CXJob(){}
    void Run(void* jobdata)    {
        printf("The Job comes from CXJOB/n");
        sleep(2);
    }
};

class CYJob:public CJob
{
public:
    CYJob(){i=0;}
    ~CYJob(){}
    void Run(void* jobdata)    {
        printf("The Job comes from CYJob/n");
    }
};

main()
{
    CThreadManage* manage = new CThreadManage(10);
    for(int i=0;i<40;i++)
    {
        CXJob*   job = new CXJob();
        manage->Run(job,NULL);
    }
    sleep(2);
    CYJob* job = new CYJob();
    manage->Run(job,NULL);
    manage->TerminateAll();
}