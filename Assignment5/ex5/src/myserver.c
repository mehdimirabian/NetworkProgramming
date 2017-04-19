
/* File Name: myserver.c
Name :     Mohammad Mirabian
Assignment:Final Project
CruzID:    mmirabia
*/

#include "myunp.h"

#include <time.h>

#define MAX_NODES 128

#define MAX_NEIGHBORS 128

#define VALUE_INFINITY 16

int update_rand = 5;
int update_interval=30;
int timeout_interval=180;
int garbage_interval=120;


/**  paramereter  */
unsigned int myIp=0;
unsigned int myPort=0;
unsigned int updatedTab=0;


struct rip_header
{
    unsigned char cmd;
    unsigned char version;
    unsigned char zero[2];
};

struct rip_entry
{
    unsigned short family;
    unsigned short zero0;
    unsigned int ip;
    unsigned int zero1;
    unsigned int zero2;
    unsigned int distance;
};

struct rip_packet_type
{
    struct rip_header header;
    struct rip_entry entry[25];
};

struct neighbor_type
{
    unsigned int virtual_ip;
    unsigned int value;
};

struct neighbor_type neighbor[MAX_NEIGHBORS]={};
unsigned int neighbors=0;


#define STATE_FREE      0
#define STATE_ACTIVE    1
#define STATE_TIMEOUT   2


struct entry_type
{
    unsigned int state;
    unsigned int dest_ip;
    unsigned int gateway_ip;
    unsigned int value;
    time_t timeout_time;
    time_t gc_time;
};

struct entry_type node[MAX_NODES]={};


void  updateValue(unsigned int gateway_ip,unsigned int ip,unsigned int value,unsigned int link_value, time_t tm)
{

    int i;
    unsigned int new_value=link_value+value;
    if(new_value>VALUE_INFINITY)
    new_value=VALUE_INFINITY;

    if(myIp==ip)return;

    for(i=0;i<MAX_NODES;i++)
    if(node[i].state && (node[i].dest_ip==ip))
    {
        if(new_value==VALUE_INFINITY)
        {

            if((gateway_ip==node[i].gateway_ip) && (node[i].value!=VALUE_INFINITY))
            {
                node[i].state=STATE_TIMEOUT;
                node[i].value=VALUE_INFINITY;
                node[i].gc_time=tm+garbage_interval;
                updatedTab=1;
            }
        }
        else
        if((gateway_ip==node[i].gateway_ip) || (node[i].value>=new_value))
        {

            if(
            (node[i].value>new_value)||(node[i].gateway_ip==gateway_ip)||
            (node[i].timeout_time-tm<timeout_interval/2)
            )
            {

                if (
                (node[i].value!=new_value)||
                (node[i].gateway_ip!=gateway_ip)
                )
                {
                    updatedTab=1;
                }
                node[i].state=STATE_ACTIVE;
                node[i].gateway_ip=gateway_ip;
                node[i].value=new_value;
                node[i].timeout_time=tm+timeout_interval;
            }
        }

        return;
    }


    if(new_value==VALUE_INFINITY)
    {
        return;
    }

    for(i=0;i<MAX_NODES;i++)
    if( !node[i].state )
    {
        node[i].dest_ip=ip;
        node[i].gateway_ip=gateway_ip;
        updatedTab=1;

        node[i].state=STATE_ACTIVE;
        node[i].value=new_value;
        node[i].timeout_time=tm+timeout_interval;

        return;
    }

}









struct mapping_type
{
    unsigned int virtual_ip;
    unsigned int virtual_port;

    unsigned int physical_ip;
    unsigned int physical_port;
};


struct mapping_type mapping[MAX_NODES];
unsigned int mappings=0;

void addNeighbour(unsigned int ip, unsigned int value)
{
    if(neighbors>=MAX_NEIGHBORS)
    err_quit("too many neighbors\n");

  

    neighbor[neighbors].virtual_ip=ip;
    neighbor[neighbors].value=value;
    neighbors++;
}

void addMapping(struct mapping_type new_maping)
{
    if(mappings>=MAX_NODES)
    {
        err_quit("Too many mappings !\n");
    }
    mapping[mappings]=new_maping;

    mappings++;
}



int phyToVirt(struct sockaddr_in * addr)
{
    int i;
    for( i=0;i<mappings;i++)
    if(
       (ntohs(addr->sin_port)==mapping[i].physical_port)&&
       (ntohl(addr->sin_addr.s_addr)==mapping[i].physical_ip)
    )
    {
       addr->sin_port=htons(mapping[i].virtual_port);
       addr->sin_addr.s_addr=htonl(mapping[i].virtual_ip);
       return 0;
    }
    return -1;
}

int virtToPhy(struct sockaddr_in * addr)
{
    int i;
    for(i=0;i<mappings;i++)
    if(
       (ntohs(addr->sin_port)==mapping[i].virtual_port)&&
       (ntohl(addr->sin_addr.s_addr)==mapping[i].virtual_ip)
    )
    {
       addr->sin_port=htons(mapping[i].physical_port);
       addr->sin_addr.s_addr=htonl(mapping[i].physical_ip);
       return 0;
    }
    return -1;
}

void readNodeConfig(const char * filename)
{
    char row[1024];

    FILE *fp=fopen(filename,"r");
    if(!fp)
    err_quit("canot open %s\n",filename);

    while(fgets(row,sizeof(row),fp)!=0)
    {
        int vip[4], vport;
        int pip[4], pport;
        struct mapping_type map;

        if(row[0]=='#')continue;

        sscanf(row,"%u.%u.%u.%u %u %u.%u.%u.%u %u",
               &vip[0],&vip[1],&vip[2],&vip[3],&vport,
               &pip[0],&pip[1],&pip[2],&pip[3],&pport
               );

        map.virtual_ip=(vip[3]<<0)+(vip[2]<<8)+(vip[1]<<16)+(vip[0]<<24);
        map.virtual_port=vport;
        map.physical_ip=(pip[3]<<0)+(pip[2]<<8)+(pip[1]<<16)+(pip[0]<<24);
        map.physical_port=pport;

        addMapping(map);
    }

    fclose(fp);
}


void readNeighborConfig(const char * filename)
{
    char row[1024];

    FILE *fp=fopen(filename,"r");
    if(!fp)
    err_quit("canot open %s\n",filename);

    while(fgets(row,sizeof(row),fp)!=0)
    {
        unsigned int sipb[4],sip;
        unsigned int dipb[4],dip;
        unsigned int v;

        if(row[0]=='#')continue;

        sscanf(row,"%u.%u.%u.%u %u.%u.%u.%u %u",
               &sipb[0],&sipb[1],&sipb[2],&sipb[3],
               &dipb[0],&dipb[1],&dipb[2],&dipb[3],
               &v
               );

        sip=(sipb[3]<<0)+(sipb[2]<<8)+(sipb[1]<<16)+(sipb[0]<<24);
        dip=(dipb[3]<<0)+(dipb[2]<<8)+(dipb[1]<<16)+(dipb[0]<<24);

        if(sip==myIp)
        addNeighbour(dip,v);

        if(dip==myIp)
        addNeighbour(sip,v);

    }

    fclose(fp);
}

int main(int argc, char **argv)
{
    int ip_parts[4],port;

    srand(time(NULL));

    if(argc<3)err_quit("Usage rserver ip port\n");

    if(argc==4)
    {
        update_rand = 2;
        update_interval=3;
        timeout_interval=18;
        garbage_interval=12;
    }

    sscanf(argv[1],"%u.%u.%u.%u",&ip_parts[0],&ip_parts[1],&ip_parts[2],&ip_parts[3]);
    sscanf(argv[2],"%u",&port);

    myIp=(ip_parts[3]<<0)+(ip_parts[2]<<8)+(ip_parts[1]<<16)+(ip_parts[0]<<24);
    myPort=port;

    readNodeConfig("node.config");
    readNeighborConfig("neighbor.config");


{
    int triggered_update_interval=0;

    int sock= Socket(AF_INET, SOCK_DGRAM, 0);

    time_t next_periodic_time;
    time_t next_trigger_time;

    time_t tstart;

    struct sockaddr_in bind_addr={};
    bind_addr.sin_addr.s_addr=htonl(myIp);
    bind_addr.sin_port=htons(myPort);

    virtToPhy(&bind_addr);

    bind_addr.sin_addr.s_addr=INADDR_ANY;

    if(-1==Bind(sock,(struct sockaddr*) &bind_addr,sizeof(bind_addr)))
        err_quit("bind failed\n");

    time(&tstart);

    next_periodic_time=tstart;
    next_trigger_time=tstart;

    while(1)
    {
        time_t next_update_time=next_periodic_time;
        int res;
        fd_set fd;

        struct timeval tm={};
        time_t next_event=next_update_time;
        time_t tnow;
        time(&tnow);

        {
            int i;

            for(i=0;i<MAX_NODES;i++)
            if(node[i].state==STATE_ACTIVE)
            {
                if(tnow-node[i].timeout_time>=0)
                {
                    node[i].state=STATE_TIMEOUT;
                    node[i].value=VALUE_INFINITY;
                    node[i].gc_time=tnow+garbage_interval;
                    updatedTab=1;
                }
            }
            else
            if(node[i].state==STATE_TIMEOUT)
            {
                if(tnow-node[i].gc_time>0)
                {
                    node[i].state=STATE_FREE;
                    updatedTab=1;
                }
            }
        }

        if(updatedTab)
        {
            int i;

            printf("%u.%u.%u.%u %u\n",
                   (myIp>>24)&0xff,(myIp>>16)&0xff,
                   (myIp>> 8)&0xff,(myIp>> 0)&0xff, myPort);


            for( i=0;i<MAX_NODES;i++)
            if(node[i].state)
            printf("starting node: %u.%u.%u.%u %u.%u.%u.%u %u\n",
                   (node[i].dest_ip>>24)&0xff,(node[i].dest_ip>>16)&0xff,
                   (node[i].dest_ip>> 8)&0xff,(node[i].dest_ip>> 0)&0xff,

                   (node[i].gateway_ip>>24)&0xff,(node[i].gateway_ip>>16)&0xff,
                   (node[i].gateway_ip>> 8)&0xff,(node[i].gateway_ip>> 0)&0xff,
                   node[i].value
                   );
            printf("\n");


            if(!triggered_update_interval)
            {
                triggered_update_interval=1+(rand()%update_rand);
                next_trigger_time=tnow+triggered_update_interval;
            }

           
            if(next_update_time-next_trigger_time>0)
            next_update_time=tnow+next_trigger_time;

            updatedTab=0;
        }

        if(next_update_time-tnow<=0)
        {
            int i,j;

            triggered_update_interval=0;

            for(i=0;i<neighbors;i++)
            {
                struct rip_packet_type p={{2,1}};

                int entries=0;
                int total=0;

                struct sockaddr_in addr={};
                addr.sin_port=htons(520);
                addr.sin_addr.s_addr=htonl(neighbor[i].virtual_ip);

                if(-1==virtToPhy(&addr))
                err_quit("could not map neighbour %08x : %d",neighbor[i].virtual_ip, 520);


                for(j=0;j<MAX_NODES;j++)
                if(
                   (node[j].state)&&
                   (node[j].dest_ip!=neighbor[i].virtual_ip))
                {
                    p.entry[entries].ip=node[j].dest_ip;
                    if(node[j].gateway_ip==neighbor[i].virtual_ip)
                    p.entry[entries].distance=VALUE_INFINITY;
                    else
                    p.entry[entries].distance=node[j].value;
                    p.entry[entries].family=2;

                    entries++;
                    total++;
                    if(entries==sizeof(p.entry)/sizeof(p.entry[0]))
                    {
                        sendto(sock,&p,sizeof(p.header)+entries*sizeof(p.entry[0]),0,(struct sockaddr*) &addr,sizeof(addr));
                        entries=0;
                    }
                }


                if(entries || (!total))
                {
                    sendto(sock,&p,sizeof(p.header)+entries*sizeof(p.entry[0]),0,(struct sockaddr*) &addr,sizeof(addr));
                }
            }

            next_periodic_time=tnow+update_interval+(rand()%update_rand);

            continue;
        }


        time(&tnow);

        {
            int i;
       
            for(i=0;i<MAX_NODES;i++)
            if(node[i].state==STATE_ACTIVE)
            {
                if(next_event-node[i].timeout_time>0)
                    next_event=node[i].timeout_time;
            }
            else
            if(node[i].state==STATE_TIMEOUT)
            {
                if(next_event-node[i].gc_time>0)
                    next_event=node[i].gc_time;
            }
        }

        if((next_event-tnow)<0)
            tm.tv_sec=0;
        else
            tm.tv_sec=(next_event-tnow);

        FD_ZERO(&fd);
        FD_SET(sock,&fd);
        res = select(sock+1,&fd,0,0,&tm);

        if(res<0)
            err_quit("select error\n");

        if(res==0)continue;

        if(FD_ISSET(sock,&fd))
        {
            int i;
            int link_value = -1;
            unsigned int destAddress;
            struct sockaddr_in addr={};
            socklen_t in_len=sizeof(addr);
            struct rip_packet_type p={};
            res = recvfrom(sock,&p,sizeof(p),0,(struct sockaddr*)&addr,&in_len);

            phyToVirt(&addr);

            destAddress=ntohl(addr.sin_addr.s_addr);


            if(res<sizeof(p.header))
                continue;


            if(p.header.cmd!=2)
                continue;

            if((res-sizeof(p.header))%sizeof(p.entry[0]))
                continue;

            {
                link_value=-1;
          
                for(i=0;i<neighbors;i++)
                {
                    if(neighbor[i].virtual_ip==destAddress)
                    link_value=neighbor[i].value;
                }

                if(link_value==-1)continue;

              
                updateValue(destAddress,destAddress,0,link_value,tnow);
            }

            for(i=0;i<(res-sizeof(p.header))/sizeof(p.entry[0]);i++)
            if(p.entry[i].family==2)
            {
                updateValue(destAddress,p.entry[i].ip,p.entry[i].distance,link_value,tnow);
            }

        }
    }

    Close(sock);
}

    return 0;
}
