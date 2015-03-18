#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "libtcod.h"

#define K_COEFF 1 /**< This COEFFICIENT is for refining forces */
#define TIME 0 /**< Min time between calculations (in ms) */
#define FPS 25
#define DEFMOUSEFORCE 100 /**< Default mouse force multiplier */
#define CHARGES_SIZE 4 /**< Number of charges */
#define X_SIZE 160 /**< Screen width in chars */
#define Y_SIZE 80 /**< Screen height in chars  */
/**< Define this, to calculate with many more charges (visible room expanded omnidirectionally), this can be refined with _ITER_ defines... */
//#define INFINTE_CHARGES
/**< Define this, to place all charges in a rectangle shape, this can be refined with STEP_ defines... */
//#define IN_A_RECTANGLE

#define MIN_ITER_X -X_SIZE/20
#define MAX_ITER_X X_SIZE/20
#define MIN_ITER_Y -Y_SIZE/20
#define MAX_ITER_Y Y_SIZE/20
/**< STEP_ defines for placing charges in a rectangle, leaving STEP_ space between each other */
#define STEP_X (X_SIZE/10)
#define STEP_Y (Y_SIZE/10)

struct _CHARGES {
    int ElecChrg;     /**< Each charges' Electric charge with sign (+/-) */
    double PosX,PosY; /**< Each charges' position (x,y) */
    double VelX,VelY; /**< Each charges' velocity (x,y) */
} *Charges;
int NumOfCharges;     /**< Number of charges currently in the room */
/** 2 Click event 3 Mouse state */
enum {MLEFTCLCK,MRIGHTCLCK} MOUSESTATE;
enum {SELECT_DEL,ADD_CHRG,MOVE_CHRG,SETVEL_CHRG} EVENTSTATE;
typedef struct {
    void (*FuncCall)(int*,int*,int*,int*,int*);
} CONTROL;
int MouseForce=DEFMOUSEFORCE; /**< Mouse force multiplier */
char FileReadOK;     /**< Flag for successful read from file */
TCOD_key_t key;      /**< Keyboard state handle */
TCOD_mouse_t mouse;  /**< Mouse state handle */
TCOD_event_t event;  /**< Event handle */
uint32 TimeStep,Time;/**< Timing handle */

/** Using Coulomb's law and Fx=F*cos(a) */
double CalculateForceX(int q1,int q2,double x0,double y0) {
    double ret=0;
#ifdef INFINTE_CHARGES
    int x,y;
    for(x=MIN_ITER_X; x<MAX_ITER_X; x++) {
        for(y=MIN_ITER_Y; y<MAX_ITER_Y; y++) {
            if(x0!=0||y0!=0||x!=0||y!=0)
                ret+=K_COEFF*q1*q2*(x0+x*X_SIZE)/( ( (x0+x*X_SIZE)*(x0+x*X_SIZE)+(y0+y*Y_SIZE)*(y0+y*Y_SIZE) ) *
                                                   sqrt( (x0+x*X_SIZE)*(x0+x*X_SIZE)+(y0+y*Y_SIZE)*(y0+y*Y_SIZE) ) );
        }
    }
#else
    if(x0!=0||y0!=0) ret+=K_COEFF*q1*q2*(x0)/( ( (x0)*(x0)+(y0)*(y0) ) * sqrt( (x0)*(x0)+(y0)*(y0) ) );
#endif // INFINTE_CHARGES
    return ret;
}
/** Using Coulomb's law and Fy=F*sin(a) */
double CalculateForceY(int q1,int q2,double x0,double y0) {
    double ret=0;
#ifdef INFINTE_CHARGES
    int x,y;
    for(x=MIN_ITER_X; x<MAX_ITER_X; x++) {
        for(y=MIN_ITER_Y; y<MAX_ITER_Y; y++) {
            if(x0!=0||y0!=0||x!=0||y!=0)
                ret+=K_COEFF*q1*q2*(y0+y*Y_SIZE)/( ( (x0+x*X_SIZE)*(x0+x*X_SIZE)+(y0+y*Y_SIZE)*(y0+y*Y_SIZE) ) *
                                                   sqrt( (x0+x*X_SIZE)*(x0+x*X_SIZE)+(y0+y*Y_SIZE)*(y0+y*Y_SIZE) ) );
        }
    }
#else
    if(x0!=0||y0!=0) ret+=K_COEFF*q1*q2*(y0)/( ( (x0)*(x0)+(y0)*(y0) ) * sqrt( (x0)*(x0)+(y0)*(y0) ) );
#endif // INFINTE_CHARGES
    return ret;
}
void DefaultState(int x1,int y1,int x2,int y2) {
    FILE *read=NULL;
    int error=1,i=0;
    if(Charges!=NULL) free(Charges);
#ifdef IN_A_RECTANGLE
    int i,x,y;
    NumOfCharges=X_SIZE/STEP_X*Y_SIZE/STEP_Y;
    Charges = (Charges*)calloc(NumOfCharges,sizeof(Charges));
    for(x=STEP_X/2,i=0; x<X_SIZE; x+=STEP_X) {
        for(y=STEP_Y/2; y<Y_SIZE; y+=STEP_Y,i++) {
            if(i>=NumOfCharges) break;
            Charges[i].PosX=x;
            Charges[i].PosY=y;
            Charges[i].VelX=Charges[i].VelY=0;
            Charges[i].ElecChrg=1;
        }
    }
#else
    read=fopen("default.ini","rt");
    FileReadOK=0;
    if(read!=NULL) {
        error=0;
        if(fscanf(read,"%d",&NumOfCharges)==1) {
            Charges = (struct _CHARGES*)calloc(NumOfCharges,sizeof(struct _CHARGES));
        } else error=1;
        while(!error && i!=NumOfCharges && fscanf(read,"%d %lf %lf",&Charges[i].ElecChrg,&Charges[i].PosX,&Charges[i].PosY)==3) {
            Charges[i].VelX=Charges[i].VelY=0;
            i++;
        }
        if(!error && i!=NumOfCharges) {
            error=1;
            free(Charges);
        }
        if(!error) FileReadOK=1;
        fclose(read);
    }
    if(error) {
        NumOfCharges=CHARGES_SIZE;
        Charges = (struct _CHARGES*)calloc(NumOfCharges,sizeof(struct _CHARGES));
        Charges[0]=(struct _CHARGES) {
            100  ,x1,y1,0,0
        };
        Charges[1]=(struct _CHARGES) {
            1000 ,x2,y2,0,0
        };
        Charges[2]=(struct _CHARGES) {
            -5000,50,50,0,0
        };
        Charges[3]=(struct _CHARGES) {
            -6   ,70,20,0,0
        };
    }
#endif // IN_A_RECTANGLE
}
void CalculateState() {
    int i,j,k,l,mx=0,my=0,bound,Coll=0;
    double dt=1.0/FPS;
    for(i=0; i<NumOfCharges; i++) {
        for(j=0; j<NumOfCharges; j++) {
            if(Charges[i].ElecChrg!=0) {
                Coll=0;
                bound=log10(abs(Charges[i].ElecChrg)+abs(Charges[j].ElecChrg));
                for(k=-bound; k<bound+1; k++) {
                    for(l=-bound; l<bound+1; l++) {
                        mx=(int)Charges[j].PosX+l;
                        my=(int)Charges[j].PosY+k;
                        if     (mx<0)       mx+=X_SIZE;
                        else if(mx>=X_SIZE) mx-=X_SIZE;
                        if     (my<0)       my+=Y_SIZE;
                        else if(my>=Y_SIZE) my-=Y_SIZE;
                        if(i!=j && my==(int)Charges[i].PosY && mx==(int)Charges[i].PosX ) Coll=1;
                    }
                }
                if(!Coll) {
                    Charges[i].VelX+=CalculateForceX(Charges[j].ElecChrg,-Charges[i].ElecChrg,Charges[j].PosX-Charges[i].PosX,Charges[j].PosY-Charges[i].PosY)/abs(Charges[i].ElecChrg)*dt;
                    Charges[i].VelY+=CalculateForceY(Charges[j].ElecChrg,-Charges[i].ElecChrg,Charges[j].PosX-Charges[i].PosX,Charges[j].PosY-Charges[i].PosY)/abs(Charges[i].ElecChrg)*dt;
                } else {
//                    Charges[i].VelX=Charges[j].VelX=(Charges[i].VelX+Charges[j].VelX)/2;
//                    Charges[i].VelY=Charges[j].VelY=(Charges[i].VelY+Charges[j].VelY)/2;
                    /* Impulse m1*v1+m2*v2=(m1+m2)*vnew */
                    Charges[i].VelX=Charges[j].VelX=(abs(Charges[i].ElecChrg)*Charges[i].VelX+abs(Charges[j].ElecChrg)*Charges[j].VelX)/( abs(Charges[i].ElecChrg)+abs(Charges[j].ElecChrg) );
                    Charges[i].VelY=Charges[j].VelY=(abs(Charges[i].ElecChrg)*Charges[i].VelY+abs(Charges[j].ElecChrg)*Charges[j].VelY)/( abs(Charges[i].ElecChrg)+abs(Charges[j].ElecChrg) );
                }
            }
        }
    }
}
void UpdateState() {
    int i;
    for(i=0; i<NumOfCharges; i++) {
        if     (Charges[i].PosX+Charges[i].VelX<0)      Charges[i].PosX+=Charges[i].VelX+X_SIZE;
        else if(Charges[i].PosX+Charges[i].VelX<X_SIZE) Charges[i].PosX+=Charges[i].VelX;
        else    Charges[i].PosX+=Charges[i].VelX-X_SIZE;
        if     (Charges[i].PosY+Charges[i].VelY<0)      Charges[i].PosY+=Charges[i].VelY+Y_SIZE;
        else if(Charges[i].PosY+Charges[i].VelY<Y_SIZE) Charges[i].PosY+=Charges[i].VelY;
        else    Charges[i].PosY+=Charges[i].VelY-Y_SIZE;
    }
}
void HelpMenu() {
    TCOD_console_set_default_background(NULL,TCOD_darker_grey);
    TCOD_console_rect(NULL,X_SIZE/2-20,Y_SIZE/2-20,40,30,1,TCOD_BKGND_SET);
    TCOD_console_set_alignment(NULL,TCOD_CENTER);
    TCOD_console_print_rect(NULL,X_SIZE/2,Y_SIZE/2-19,40,30,"- INSTRUCTIONS -\n\n\n"
                            "      H  -  HELP          \n"
                            "      P  -  PAUSE         \n"
                            "      R  -  RESET         \n"
                            "      D  -  SET DELAY     \n"
                            "      M  -  SET MOUSEFORCE\n\n"
                            "   UPARROW - INCREMENT DELAY BY 1     \n"
                            " DOWNARROW - DECREMENT DELAY BY 1     \n"
                            "RIGHTARROW - INCREMENT MOUSEFORCE BY 1\n"
                            " LEFTARROW - DECREMENT MOUSEFORCE BY 1\n\n"
                            " Left mouse button  attracts charges\n"
                            "Right mouse button distracts charges\n\n\n"
                            "-In pause-\n"
                            "Set the mouse mode with the mwheel\n"
                            "Default mouse mode is select/del  \n\n\n"
                            "--In default.ini file--\n"
                            "        1.line: Number of charges   \n"
                            "   other lines: ChargeSize X0 Y0    \n\n\n"
                            "                     Made by: Akos Kote \n"
                            "                            2015 - v1.0 \n");
    TCOD_console_set_alignment(NULL,TCOD_LEFT);
}
void RadiusEffect(int x, int y, double r,TCOD_color_t col) {
    int i,j;
    double rad;
    if(r>0) {
        r=log(r)/log(1.3);
        for(i=-r; i<r+1; i++) {
            for(j=-r; j<r+1; j++) {
                if(i!=0||j!=0) rad=sqrt(i*i+j*j);
                else           rad=1;
                if(x+i>=0&&y+j>=0&&x+i<X_SIZE&&y+j<Y_SIZE&&rad<=r) {
                    TCOD_console_set_char_background(NULL,x+i,y+j,col,TCOD_BKGND_ADDALPHA( (1.0-rad/r)*0.25*(1.0-1.0/r) ));
                }
            }
        }
    }
}
void PrintDownRightStatus() {
    TCOD_console_set_default_background(NULL,TCOD_darker_grey);
    TCOD_console_set_alignment(NULL,TCOD_RIGHT);
    TCOD_console_print(NULL,X_SIZE-1,Y_SIZE-3,"MFORCE: %-4d  ",MouseForce);
    TCOD_console_print(NULL,X_SIZE-1,Y_SIZE-2,"FPS   : %-d    ",FPS);
    TCOD_console_print(NULL,X_SIZE-1,Y_SIZE-1,"DELAY : %04dms",TimeStep);
    TCOD_console_set_alignment(NULL,TCOD_LEFT);
    TCOD_console_set_default_background(NULL,TCOD_black);
}
void PrintState() {
    int i,j;
    TCOD_console_set_default_foreground(NULL,TCOD_white);
    TCOD_console_set_default_background(NULL,TCOD_black);
    TCOD_console_clear(NULL);
    for(i=0; i<NumOfCharges; i++) {
        if(Charges[i].ElecChrg>=0) {
            TCOD_console_set_default_foreground(NULL,TCOD_red);
            RadiusEffect(Charges[i].PosX,Charges[i].PosY,Charges[i].ElecChrg,TCOD_red);
        } else {
            TCOD_console_set_default_foreground(NULL,TCOD_blue);
            RadiusEffect(Charges[i].PosX,Charges[i].PosY,-Charges[i].ElecChrg,TCOD_blue);
        }
        TCOD_console_put_char(NULL,Charges[i].PosX,Charges[i].PosY,'@',TCOD_BKGND_NONE);
    }
    if     (mouse.lbutton) RadiusEffect(mouse.cx,mouse.cy,MouseForce,TCOD_green);
    else if(mouse.rbutton) RadiusEffect(mouse.cx,mouse.cy,MouseForce,TCOD_yellow);
    TCOD_console_set_default_foreground(NULL,TCOD_white);
    if(TCOD_sys_elapsed_milli()<=6000) {
        HelpMenu();
        if(FileReadOK) TCOD_console_print(NULL,0,0,"Successful file read from default.ini");
        else           TCOD_console_print(NULL,0,0,"File read from default.ini has failed! Loading default state...");
    }
    TCOD_console_set_background_flag(NULL,TCOD_BKGND_SET);
    for(i=0,j=0; i<NumOfCharges; i++,j+=2) {
        if(Charges[i].ElecChrg>=0) TCOD_console_set_default_background(NULL,TCOD_red);
        else                       TCOD_console_set_default_background(NULL,TCOD_blue);
        TCOD_console_print(NULL,X_SIZE-27,j ,"%02d.%+5dx X vel: % .6f ",i+1,Charges[i].ElecChrg,Charges[i].VelX);
        TCOD_console_print(NULL,X_SIZE-27,j+1,"   %+5dx Y vel: % .6f ",Charges[i].ElecChrg,Charges[i].VelY);
    }
    PrintDownRightStatus();
    TCOD_console_flush();
}
/** Halt program for delay (TimeStep) amount (in ms) */
inline void Wait(uint32*Time) {
    uint32 CurrentTime = TCOD_sys_elapsed_milli();
    if(*Time>CurrentTime) TCOD_sys_sleep_milli(*Time-CurrentTime);
    *Time=TCOD_sys_elapsed_milli()+TimeStep;
}
inline void DisplayPause() {
    TCOD_console_set_alignment(NULL,TCOD_CENTER);
    TCOD_console_set_default_background(NULL,TCOD_red);
    TCOD_console_print(NULL,X_SIZE/2,Y_SIZE-1,"-== PAUSED ==-");
    TCOD_console_set_default_background(NULL,TCOD_black);
    TCOD_console_set_alignment(NULL,TCOD_LEFT);
}
inline void DisplayMState() {
    TCOD_console_set_alignment(NULL,TCOD_CENTER);
    TCOD_console_set_default_background(NULL,TCOD_blue);
    switch(EVENTSTATE) {
    case SELECT_DEL:
        TCOD_console_print(NULL,X_SIZE/2,Y_SIZE-2,"-== Select/Delete ==-");
        break;
    case ADD_CHRG:
        TCOD_console_print(NULL,X_SIZE/2,Y_SIZE-2,"-== Add Charge ==-");
        break;
    case MOVE_CHRG:
        TCOD_console_print(NULL,X_SIZE/2,Y_SIZE-2,"-== Move Charge ==-");
        break;
    case SETVEL_CHRG:
        TCOD_console_print(NULL,X_SIZE/2,Y_SIZE-2,"-== Set Charge velocity ==-");
        break;
    }
    TCOD_console_set_default_background(NULL,TCOD_black);
    TCOD_console_set_alignment(NULL,TCOD_LEFT);
}
inline void Reset() {
    DefaultState(15,15,80,50);
}
/** Input handle function */
int InputHandle(TCOD_key_t *key,const char *text,const char *suff) {
    int Input=0,Sign=1;
    do {
        TCOD_console_rect(NULL,0,0,41,2,1,TCOD_BKGND_DEFAULT);
        if(Input>=10000) {
            Input=0;
            TCOD_console_set_default_background(NULL,TCOD_red);
            TCOD_console_print(NULL,0,1,"TOO BIG - try again - should be <10000%s ",suff);
            TCOD_console_set_default_background(NULL,TCOD_black);
        }
        TCOD_console_print(NULL,0,0,"%s",text);
        if(Sign==1) TCOD_console_print(NULL,strlen(text),0,"%d%s",Input,suff);
        else        TCOD_console_print(NULL,strlen(text),0,"-%d%s",Input,suff);
        TCOD_console_flush();
        TCOD_sys_wait_for_event(TCOD_EVENT_KEY_PRESS,key,NULL,1);
        if(key->c>='0'&&key->c<='9') Input=(key->c-'0')+Input*10;
        else if(key->c=='-') Sign*=-1;
        else if(key->vk==TCODK_DELETE||key->vk==TCODK_BACKSPACE) Input=0;
    } while(key->vk!=TCODK_ENTER&&key->vk!=TCODK_KPENTER&&key->vk!=TCODK_ESCAPE&&!TCOD_console_is_window_closed());
    TCOD_sys_wait_for_event(TCOD_EVENT_KEY_RELEASE,NULL,NULL,1);
    return Sign*Input;
}
void SetDelay(uint32 *TimeStep) {
    TCOD_key_t key;
    int Delay=0;
    DisplayPause();
    Delay = InputHandle(&key,"Enter new delay (ms): ","ms");
    if(key.vk!=TCODK_ESCAPE) *TimeStep=Delay;
}
void SetMouseForce(int *MouseForce) {
    TCOD_key_t key;
    int Force=0;
    DisplayPause();
    Force = InputHandle(&key,"Enter new mouse force: "," ");
    if(key.vk!=TCODK_ESCAPE) *MouseForce=Force;
}
void MouseEvent() {
    int i;
    if(mouse.lbutton) {
        for(i=0; i<NumOfCharges; i++) {
            if(Charges[i].ElecChrg!=0) {
                Charges[i].VelX+=CalculateForceX(MouseForce,Charges[i].ElecChrg,mouse.cx-Charges[i].PosX,mouse.cy-Charges[i].PosY)/Charges[i].ElecChrg*1.0/FPS;
                Charges[i].VelY+=CalculateForceY(MouseForce,Charges[i].ElecChrg,mouse.cx-Charges[i].PosX,mouse.cy-Charges[i].PosY)/Charges[i].ElecChrg*1.0/FPS;
            }
        }
    } else if(mouse.rbutton) {
        for(i=0; i<NumOfCharges; i++) {
            if(Charges[i].ElecChrg!=0) {
                Charges[i].VelX+=CalculateForceX(-MouseForce,Charges[i].ElecChrg,mouse.cx-Charges[i].PosX,mouse.cy-Charges[i].PosY)/Charges[i].ElecChrg*1.0/FPS;
                Charges[i].VelY+=CalculateForceY(-MouseForce,Charges[i].ElecChrg,mouse.cx-Charges[i].PosX,mouse.cy-Charges[i].PosY)/Charges[i].ElecChrg*1.0/FPS;
            }
        }
    }
}
/** Halt program run for delay (TimeStep) amount (in ms) or until keypress event */
void CheckForEvent(uint32 *Time,TCOD_key_t *key,TCOD_mouse_t *mouse,TCOD_event_t *event) {
    do {
        *event=TCOD_sys_check_for_event(TCOD_EVENT_ANY,key,mouse);
        TCOD_sys_sleep_milli(1000/FPS);
    } while(*Time>TCOD_sys_elapsed_milli()&&*event!=TCOD_EVENT_KEY_PRESS&&!TCOD_console_is_window_closed());
    *Time=TCOD_sys_elapsed_milli()+TimeStep;
    if(*event==TCOD_EVENT_MOUSE_PRESS||TCOD_EVENT_MOUSE_MOVE) MouseEvent();
}
inline void RefreshPauseScreen() {
    PrintDownRightStatus();
    TCOD_console_flush();
    TCOD_sys_wait_for_event(TCOD_EVENT_KEY,NULL,NULL,1); /*Catch key event, clear input buffer*/
}
inline void ReprintPauseScreen() {
    mouse.lbutton=mouse.rbutton=0; /*Disable radius effect print*/
    PrintState();
    DisplayPause();
    DisplayMState();
    TCOD_console_flush();
}
void MSelectDelLeft(int* prevX,int* prevY,int* sel,int* ElecChrg,int* indx) {
    int i;
    for(i=0,*sel=0; i<NumOfCharges; i++) {
        if(*prevX==(int)Charges[i].PosX&&*prevY==(int)Charges[i].PosY) {
            if(Charges[i].ElecChrg>=0) {
                TCOD_console_set_char_foreground(NULL,Charges[i].PosX,Charges[i].PosY,TCOD_red);
                TCOD_console_set_default_background(NULL,TCOD_red);
            } else {
                TCOD_console_set_char_foreground(NULL,Charges[i].PosX,Charges[i].PosY,TCOD_blue);
                TCOD_console_set_default_background(NULL,TCOD_blue);
            }
            TCOD_console_rect(NULL,X_SIZE-27,i*2,27,2,0,TCOD_BKGND_SET);
        }
        if(mouse.cx==(int)Charges[i].PosX&&mouse.cy==(int)Charges[i].PosY) {
            TCOD_console_set_char_foreground(NULL,Charges[i].PosX,Charges[i].PosY,TCOD_green);
            TCOD_console_set_default_background(NULL,TCOD_darker_green);
            TCOD_console_rect(NULL,X_SIZE-27,i*2,27,2,0,TCOD_BKGND_SET);
            *sel=1;
        }
    }
    *prevX=mouse.cx;
    *prevY=mouse.cy;
    TCOD_console_flush();
}
void MSelectDelRight(int* prevX,int* prevY,int* sel,int* ElecChrg,int* indx) {
    int i,j,Enable=0;
    struct _CHARGES* temp;
    if(*sel==0) {
        for(i=0; i<NumOfCharges; i++) {
            if(mouse.cx==(int)Charges[i].PosX&&mouse.cy==(int)Charges[i].PosY) {
                *sel=1;
                *prevX=mouse.cx;
                *prevY=mouse.cy;
            }
        }
    }
    if(*sel==1) {
        temp=(struct _CHARGES*)calloc(NumOfCharges-1,sizeof(struct _CHARGES));
        for(i=0,j=0; i<NumOfCharges; i++) {
            if(!(*prevX==(int)Charges[i].PosX&&*prevY==(int)Charges[i].PosY) || Enable ) {
                temp[j]=Charges[i];
                j++;
            } else Enable=1;
        }
        NumOfCharges--;
        free(Charges);
        Charges=temp;
        ReprintPauseScreen();
        *sel=0;
    }
}
void MAddCharge(int* prevX,int* prevY,int* sel,int* ElecChrg,int* indx) {
    int i;
    struct _CHARGES* temp;
    if(*ElecChrg==0) {
        TCOD_sys_wait_for_event(TCOD_EVENT_MOUSE_RELEASE,NULL,NULL,1); /* Catch release event */
        *ElecChrg=InputHandle(&key,"Enter electric charge size: ","x");
    }
    temp=(struct _CHARGES*)calloc(NumOfCharges+1,sizeof(struct _CHARGES));
    for(i=0; i<NumOfCharges; i++)
        temp[i]=Charges[i];
    if     (mouse.lbutton) temp[i]=(struct _CHARGES) {
        *ElecChrg ,mouse.cx,mouse.cy,0,0
    };
    else if(mouse.rbutton) temp[i]=(struct _CHARGES) {
        -(*ElecChrg),mouse.cx,mouse.cy,0,0
    };
    NumOfCharges++;
    free(Charges);
    Charges=temp;
    ReprintPauseScreen();
}
void MMoveCharge(int* prevX,int* prevY,int* sel,int* ElecChrg,int* indx) {
    int i;
    TCOD_mouse_t mouse;
    do {
        event = TCOD_sys_check_for_event(TCOD_EVENT_MOUSE,NULL,&mouse);
        for(i=0; i<NumOfCharges; i++) {
            if(mouse.cx==(int)Charges[i].PosX&&mouse.cy==(int)Charges[i].PosY) *indx=i;
        }
        Charges[*indx].PosX=mouse.cx;
        Charges[*indx].PosY=mouse.cy;
        ReprintPauseScreen();
        TCOD_sys_sleep_milli(1000/FPS);
    } while(mouse.lbutton);
}
void MSetVelCharge(int* prevX,int* prevY,int* sel,int* ElecChrg,int* indx) {
    int i,Xvel=-1,Yvel=-1;
    TCOD_key_t key;
    TCOD_sys_wait_for_event(TCOD_EVENT_MOUSE_RELEASE,NULL,NULL,1); /* Catch release event */
    for(i=0; i<NumOfCharges; i++) {
        if(mouse.cx==(int)Charges[i].PosX&&mouse.cy==(int)Charges[i].PosY) {
            Xvel=InputHandle(&key,"Set x velocity: "," ");
            if(key.vk!=TCODK_ESCAPE) Charges[i].VelX=Xvel;
            Yvel=InputHandle(&key,"Set y velocity: "," ");
            if(key.vk!=TCODK_ESCAPE) Charges[i].VelY=Yvel;
        }
    }
    ReprintPauseScreen();
}
inline void InitControl(CONTROL Control[2][4]) {
    Control [MLEFTCLCK][SELECT_DEL] .FuncCall=MSelectDelLeft;
    Control [MLEFTCLCK][ADD_CHRG]   .FuncCall=MAddCharge;
    Control [MLEFTCLCK][MOVE_CHRG]  .FuncCall=MMoveCharge;
    Control [MLEFTCLCK][SETVEL_CHRG].FuncCall=MSetVelCharge;
    Control[MRIGHTCLCK][SELECT_DEL] .FuncCall=MSelectDelRight;
    Control[MRIGHTCLCK][ADD_CHRG]   .FuncCall=MAddCharge;
    Control[MRIGHTCLCK][MOVE_CHRG]  .FuncCall=NULL;
    Control[MRIGHTCLCK][SETVEL_CHRG].FuncCall=NULL;
}
void Pause(TCOD_key_t *key,TCOD_mouse_t *mouse,TCOD_event_t *event) {
    int prevX=0,prevY=0,sel=0,ElecChrg=0,indx=0;
    CONTROL Control[2][4]; /*2 mouse click,4 event state*/
    EVENTSTATE=SELECT_DEL;
    MOUSESTATE=0;
    InitControl(Control);
    ReprintPauseScreen();
    /* Delay after pressing p */
    do {
        *event = TCOD_sys_check_for_event(TCOD_EVENT_KEY_RELEASE,key,NULL);
        TCOD_sys_sleep_milli(1000/FPS);
    } while(key->pressed);
    do {
        TCOD_sys_sleep_milli(1000/FPS);
        *event = TCOD_sys_check_for_event(TCOD_EVENT_ANY,key,mouse);

        if(*event==TCOD_EVENT_MOUSE_PRESS) {
            if     (mouse->lbutton) MOUSESTATE=MLEFTCLCK;
            else if(mouse->rbutton) MOUSESTATE=MRIGHTCLCK;
            if(Control[MOUSESTATE][EVENTSTATE].FuncCall!=NULL) Control[MOUSESTATE][EVENTSTATE].FuncCall(&prevX,&prevY,&sel,&ElecChrg,&indx);
        } else if(*event==TCOD_EVENT_KEY_PRESS&&key->vk==TCODK_DELETE&&sel) {
            MSelectDelRight(&prevX,&prevY,&sel,&ElecChrg,&indx);
        }
        if (mouse->wheel_up) {
            if(EVENTSTATE<SETVEL_CHRG) EVENTSTATE++;
            else                       EVENTSTATE=SELECT_DEL;
            ReprintPauseScreen();
        } else if(mouse->wheel_down) {
            if(EVENTSTATE>SELECT_DEL) EVENTSTATE--;
            else                      EVENTSTATE=SETVEL_CHRG;
            ReprintPauseScreen();
        }
        if     (key->vk==TCODK_UP)    {
            TimeStep++;
            RefreshPauseScreen();
        } else if(key->vk==TCODK_DOWN)  {
            TimeStep--;
            RefreshPauseScreen();
        } else if(key->vk==TCODK_RIGHT) {
            MouseForce++;
            RefreshPauseScreen();
        } else if(key->vk==TCODK_LEFT)  {
            MouseForce--;
            RefreshPauseScreen();
        }
    } while(!(key->c=='p'&&*event==TCOD_EVENT_KEY_RELEASE)&&key->vk!=TCODK_ESCAPE&&!TCOD_console_is_window_closed());
}
void Help(TCOD_key_t *key) {
    DisplayPause();
    HelpMenu();
    TCOD_console_flush();
    /* Delay after pressing h */
    do {
        TCOD_sys_check_for_event(TCOD_EVENT_KEY_RELEASE,key,NULL);
        TCOD_sys_sleep_milli(1000/FPS);
    } while(key->pressed);
    TCOD_sys_wait_for_event(TCOD_EVENT_KEY_RELEASE,key,NULL,1); /*Wait for any key release*/
}

int main(int argc,char* argv[]) {
    TCOD_console_init_root(X_SIZE,Y_SIZE,"Universe",0,TCOD_RENDERER_SDL);
    TCOD_sys_set_fps(FPS);
    TimeStep=Time=TIME;
    Charges=NULL;
    Reset();
    do {
        CalculateState();
        UpdateState();
        CheckForEvent(&Time,&key,&mouse,&event);
        PrintState();
        if     (key.c=='p') Pause(&key,&mouse,&event);
        else if(key.c=='r') Reset();
        else if(key.c=='d') SetDelay(&TimeStep);
        else if(key.c=='m') SetMouseForce(&MouseForce);
        else if(key.c=='h') Help(&key);
        else if(key.vk==TCODK_UP)   TimeStep++;
        else if(key.vk==TCODK_DOWN) TimeStep--;
        else if(key.vk==TCODK_RIGHT)MouseForce++;
        else if(key.vk==TCODK_LEFT) MouseForce--;
    } while(key.vk!=TCODK_ESCAPE&&!TCOD_console_is_window_closed());
    return 0;
}
