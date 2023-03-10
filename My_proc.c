/* 

 * procfs2.c -  create a "file" in /proc 

 */ 

 

#include <linux/kernel.h> /* We're doing kernel work */ 
#include <linux/module.h> /* Specifically, a module */ 
#include <linux/proc_fs.h> /* Necessary because we use the proc fs */ 
#include <linux/uaccess.h> /* for copy_from_user */ 
#include <linux/sched.h> /* task structure */
#include <linux/version.h> 

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0) 
#define HAVE_PROC_OPS 
#endif 


#define PROCFS_MAX_SIZE 1024 
#define OUTPUT_MAX_SIZE 4096 
#define PROCFS_NAME "thread_info" 

/* This structure hold information about the /proc file */ 
static struct proc_dir_entry *our_proc_file; 

/* The buffer used to store character for this module */ 
static char procfs_buffer[PROCFS_MAX_SIZE]; 

/* Store what should be output */
static char thread_storage[OUTPUT_MAX_SIZE];

/* The size of the buffer */ 
static unsigned long procfs_buffer_size = 0; 

/* This function is called then the /proc file is read */ 
static ssize_t procfs_read(struct file *filePointer, char __user *buffer, size_t buffer_length, loff_t *offset) //only user read will enter this
{ 
    int len = strlen(thread_storage);
    ssize_t ret = len; 

    if (*offset >= len || copy_to_user(buffer, thread_storage, len)) { 
        pr_info("copy_to_user failed\n"); 
        ret = 0; 
    } else { 
        pr_info("procfile read %s\n", filePointer->f_path.dentry->d_name.name); 
        *offset += len; 
    } 

    return ret; 
} 



/* This function is called with the /proc file is written. */ 
static ssize_t procfs_write(struct file *file, const char __user *buff, size_t len, loff_t *off) 
{ 
    pr_info("len is %d\n",len);
    char buffer[100];
    if (copy_from_user(buffer, buff, len)){ //return 1 means error ,the count why have to be len ,if be 100 will make what different?
        return -EFAULT; 
    }
    else{ //get runtime and context switch time of the thread
        buffer[len] = '\0';
        pr_info("write%s\n",buffer);
        int tid = 0, cst;
        long long rt;

        // kstrtol(buffer,10,tid); //string to integer ,how to use this
        int base = 1, i = len - 1;
        while(i>=0){
            tid += base * (buffer[i--] - '0');
            base *= 10;
        }

        /* get runtime and context switch time */
        pr_info("tid is %d\n",tid);
        struct task_struct *t;
        struct pid *p = find_get_pid(tid);
        if(p) t = pid_task(p,PIDTYPE_PID);
        if(t){
           rt = t->utime;
           rt /= 1000000;
           cst = t->nvcsw + t->nivcsw;
           char tmp[100];
           sprintf(tmp,"    ThreadID:%d Time:%lld(ms) context switch times:%d\n",tid,rt,cst);
           strcat(thread_storage, tmp);
           thread_storage[strlen(thread_storage)] = '\0';
        }
        else
            pr_info("error in finding task!!\n");

    }

    procfs_buffer[procfs_buffer_size & (PROCFS_MAX_SIZE - 1)] = '\0'; 
    *off += procfs_buffer_size; 
    pr_info("procfile write %s\n", procfs_buffer); 
    return procfs_buffer_size; 
} 

static int procfs_open(struct inode *inode, struct file *file) 
{ 
    thread_storage[0] = '\0';
    try_module_get(THIS_MODULE); 
    return 0; 
} 

static int procfs_close(struct inode *inode, struct file *file) 
{ 
    module_put(THIS_MODULE); 
    return 0; 
} 

 
#ifdef HAVE_PROC_OPS 
static struct proc_ops proc_file_fops = { 
    .proc_read = procfs_read, 
    .proc_write = procfs_write, 
    .proc_open = procfs_open, 
    .proc_release = procfs_close, 
}; 

#else 
static const struct file_operations proc_file_fops = { 
    .read = procfs_read, 
    .write = procfs_write, 
    .open = procfs_open, 
    .release = procfs_close, 
}; 

#endif 

static int __init procfs2_init(void) 
{ 
    thread_storage[0] ='\0';
    our_proc_file = proc_create(PROCFS_NAME, 0666, NULL, &proc_file_fops); //0666 let user can write ,0644 can't
    if (NULL == our_proc_file) { 
        proc_remove(our_proc_file); 
        pr_alert("Error:Could not initialize /proc/%s\n", PROCFS_NAME); 
        return -ENOMEM; 
    }  

    pr_info("/proc/%s created\n", PROCFS_NAME);
    return 0; 
} 

static void __exit procfs2_exit(void) 
{ 
    proc_remove(our_proc_file); 
    pr_info("/proc/%s removed\n", PROCFS_NAME); 
} 

module_init(procfs2_init); 
module_exit(procfs2_exit); 

MODULE_LICENSE("GPL");
