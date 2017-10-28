#include<stdio.h>  
#include<sys/types.h>  
int main(int argc,char *argv[])  
{  
    int humidityfd;  
    int ret;  
    char buf[5];  
    unsigned char  tempz = 0;  
    unsigned char  tempx = 0;       
    unsigned char  humidiyz = 0;  
    unsigned char  humidiyx = 0;      
    humidityfd = open("/dev/DHT11_DEVICE",0);  
    if(humidityfd<0){  
        printf("/dev/humidiy open fail\n");  
        return 0;       
	}                             
    while(1){         
    ret=read(humidityfd,buf,sizeof(buf));  
                if(ret<0)  
                printf("read err!\n");  
                else{  
        humidiyz  =buf[0];    
        humidiyx  =buf[1];    
        tempz     =buf[2];       
        tempx     =buf[3];        
        printf("humidity = %d.%d%%\n", humidiyz, humidiyx);  
        printf("temp = %d.%d\n",tempz,tempx);     
                }  
        sleep(2);  
    }  
    close(humidityfd);  
    return 0;  
}  