/* 
   Compile with:
   g++ -o Space2Ctrl Space2Ctrl.cpp -W -Wall -L/usr/X11R6/lib -lX11 -lXtst

   To install libx11:
   in Ubuntu: sudo apt-get install libx11-dev

   To install libXTst:
   in Ubuntu: sudo apt-get install libxtst-dev

   Needs module XRecord installed. To install it, add line Load "record" to Section "Module" in
   /etc/X11/xorg.conf like this:

   Section "Module"
   Load  "record"
   EndSection

*/

#include <iostream>
#include <X11/Xlibint.h>
#include <X11/keysym.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>
#include <sys/time.h>
#include <signal.h>
#include <map>

using namespace std;

struct CallbackClosure {
    Display *ctrlDisplay;
    Display *dataDisplay;
    int curX;
    int curY;
    void *initialObject;
};

typedef union {
    unsigned char type;
    xEvent event;
    xResourceReq req;
    xGenericReply reply;
    xError error;
    xConnSetupPrefix setup;
} XRecordDatum;

class Key
{
public:
    int id;  // id that is pressed
    int r_id;  // replacement id
    int a_id;  // alternate id (such as ctrl-left, alt-right, etc)
    bool down;
    struct timeval start;
    Key(int id, int r_id, int a_id): id(id), r_id(r_id), a_id(a_id), down(false){
    }
    // virtual ~Key();
};


class Space2Ctrl {

    string m_displayName;
    CallbackClosure userData;
    std::pair<int,int> recVer;
    XRecordRange *recRange;
    XRecordClientSpec recClientSpec;
    XRecordContext recContext;

    void setupXTestExtension(){
        int ev, er, ma, mi;
        if (!XTestQueryExtension(userData.ctrlDisplay, &ev, &er, &ma, &mi)) {
            cout << "%sThere is no XTest extension loaded to X server.\n" << endl;
            throw exception();
        }
    }

    void setupRecordExtension() {
        XSynchronize(userData.ctrlDisplay, True);

        // Record extension exists?
        if (!XRecordQueryVersion(userData.ctrlDisplay, &recVer.first, &recVer.second)) {
            cout << "%sThere is no RECORD extension loaded to X server.\n"
                "You must add following line:\n"
                "   Load  \"record\"\n"
                "to /etc/X11/xorg.conf, in section `Module'.%s" << endl;
            throw exception();
        }

        recRange = XRecordAllocRange ();
        if (!recRange) {
            // "Could not alloc record range object!\n";
            throw exception();
        }
        recRange->device_events.first = KeyPress;
        recRange->device_events.last = ButtonRelease;
        recClientSpec = XRecordAllClients;

        // Get context with our configuration
        recContext = XRecordCreateContext(userData.ctrlDisplay, 0, &recClientSpec, 1, &recRange, 1);
        if (!recContext) {
            cout << "Could not create a record context!" << endl;
            throw exception();
        }
    }

    static int diff_ms(timeval t1, timeval t2) {
        return ( ((t1.tv_sec - t2.tv_sec) * 1000000)
                 + (t1.tv_usec - t2.tv_usec) ) / 1000;
    }

    // Called from Xserver when new event occurs.
    static void eventCallback(XPointer priv, XRecordInterceptData *hook) {

        if (hook->category != XRecordFromServer) {
            XRecordFreeData(hook);
            return;
        }

        CallbackClosure *userData = (CallbackClosure *) priv;
        XRecordDatum *data = (XRecordDatum *) hook->data;
        static Key k_o = Key(39, 252, XK_Control_L);
        static Key k_n = Key(46, 253, XK_Control_R);
        static std::map<int,Key*> ctrls{{k_o.id, &k_o},
                {k_n.id, &k_n}};
        // static bool k_o.down = false;
        // static bool key_combo = false;
        static bool c_right= false, c_left=false, a_left = false;
        static bool r_ctrl, f_ctrl, r_alt, f_alt; // real and fake control and alt
        // static bool modifier_down = false;
        // static struct timeval startWait, endWait;

        unsigned char t = data->event.u.u.type;
        int c = data->event.u.u.detail;

        // cout << "\nState:" << c << endl;
        // if (k_o.down)
        //   cout << "k_o.down = true" << endl;
        // else
        //   cout << "k_o.down = false" << endl;

        // if (key_combo)
        //   cout << "key_combo = true" << endl;
        // else
        //   cout << "key_combo = false" << endl;

        // // if (modifier_down)
        // //   cout << "modifier_down = true" << endl;
        // // else
        // //   cout << "modifier_down = false" << endl;
        // cout << endl;

        switch (t) {
        case KeyPress:
            {
                // verify the first modifiers
                if(c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_L))
                    c_left = true;
                else if(c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R))
                    c_right = true;
                else if(c == XKeysymToKeycode(userData->ctrlDisplay, XK_Alt_L))
                    a_left = true;
                // get control and r_alt variable
                r_ctrl = c_left||c_right;
                f_ctrl = k_o.down||k_n.down; // for now it is easier one by one
                if (ctrls.count(c)!=0 && r_ctrl){
                    XTestFakeKeyEvent(userData->ctrlDisplay, ctrls[c]->r_id,
                                      True, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, ctrls[c]->r_id,
                                      False, CurrentTime);
                }
                else if (ctrls.count(c)!=0 && f_ctrl){
                    XTestFakeKeyEvent(userData->ctrlDisplay, XK_Control_L,
                                      True, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, ctrls[c]->r_id,
                                      True, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, ctrls[c]->r_id,
                                      False, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, XK_Control_L,
                                      False, CurrentTime);
                } else if (ctrls.count(c)!=0){
                    ctrls[c]->down=true;
                }


                break;
            }
        case KeyRelease:
            {
                if(c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_L))
                    c_left = false;
                else if(c == XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R))
                    c_right = false;
                else if(c == XKeysymToKeycode(userData->ctrlDisplay, XK_Alt_L))
                    a_left = false;
                else if(c==k_o.id)
                    k_o.down=false;
                else if(ctrls.count(c)!=0)
                    ctrls[c]->down=false;
                break;
            }
        case ButtonPress:
            {

                // if (k_o.down) {
                //     key_combo = true;
                // } else {
                //     key_combo = false;
                // }

                // break;
            }
        }
        if (k_o.down)
          cout << "k_o.down = true" << endl;
        else
          cout << "k_o.down = false" << endl;
        if (k_n.down)
          cout << "k_n.down = true" << endl;
        else
          cout << "k_n.down = false" << endl;

        XRecordFreeData(hook);
    }

public:
    Space2Ctrl() { }
    ~Space2Ctrl() {
        stop();
    }

    bool connect(string displayName) {
        m_displayName = displayName;
        if (NULL == (userData.ctrlDisplay = XOpenDisplay(m_displayName.c_str())) ) {
            return false;
        }
        if (NULL == (userData.dataDisplay = XOpenDisplay(m_displayName.c_str())) ) {
            XCloseDisplay(userData.ctrlDisplay);
            userData.ctrlDisplay = NULL;
            return false;
        }

        // You may want to set custom X error handler here

        userData.initialObject = (void *) this;
        setupXTestExtension();
        setupRecordExtension();

        return true;
    }

    void start() {
        // // Remap keycode 255 to Keysym space:
        // KeySym spc = XK_space;
        // XChangeKeyboardMapping(userData.ctrlDisplay, 255, 1, &spc, 1);
        // XFlush(userData.ctrlDisplay);

        // TODO: document why the following event is needed
        XTestFakeKeyEvent(userData.ctrlDisplay, 255, True, CurrentTime);
        XTestFakeKeyEvent(userData.ctrlDisplay, 255, False, CurrentTime);

        if (!XRecordEnableContext(userData.dataDisplay, recContext, eventCallback,
                                  (XPointer) &userData)) {
            throw exception();
        }
    }

    void stop() {
        if (!XRecordDisableContext (userData.ctrlDisplay, recContext)) {
            throw exception();
        }
    }

};

Space2Ctrl* space2ctrl;

void stop(int param) {
    delete space2ctrl;
    if(param == SIGTERM)
        cout << "-- Terminating Space2Ctrl --" << endl;
    exit(1);
}

int main() {
    cout << "-- Starting Space2Ctrl --" << endl;
    space2ctrl = new Space2Ctrl();

    void (*prev_fn)(int);

    prev_fn = signal (SIGTERM, stop);
    if (prev_fn==SIG_IGN) signal (SIGTERM,SIG_IGN);

    if (space2ctrl->connect(":0")) {
        space2ctrl->start();
    }
    return 0;
}
