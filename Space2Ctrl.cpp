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
#include <map>
#include <list>
#include <algorithm>

#include <X11/Xlibint.h>
#include <X11/keysym.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>
#include <sys/time.h>
#include <signal.h>

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

typedef std::pair<int,bool> key_pair;
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

    static bool hit_or_mod(std::map<int,Key*> keymaps, int c){
        return  std::accumulate(keymaps.begin(), keymaps.end(), false,
                                [c] (bool value, const std::map<int, Key*>::value_type& p)
                                { return value ||  (p.second->down &&  p.second->id != c); });

    }

    static bool anydown(std::map<int,Key*> keymaps){
        return  std::accumulate(keymaps.begin(), keymaps.end(), false,
                                [] (bool value, const std::map<int, Key*>::value_type& p)
                                { return value ||  p.second->down; });

    }

    // Called from Xserver when new event occurs.
    static void eventCallback(XPointer priv, XRecordInterceptData *hook) {
        static std::list<key_pair> fakes;

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
        static int c_left_id = XKeysymToKeycode(userData->ctrlDisplay, XK_Control_L);
        static int c_right_id = XKeysymToKeycode(userData->ctrlDisplay, XK_Control_R);
        static int backspace_id = XKeysymToKeycode(userData->ctrlDisplay, XK_BackSpace);
        unsigned char t = data->event.u.u.type;
        int c = data->event.u.u.detail;
        // short
        int key = ((unsigned char*) hook->data)[1];
        int type = ((unsigned char*) hook->data)[0] & 0x7F;
        int repeat = hook->data[2] & 1;

        // verify that the pressed key is not in the fake keys
        key_pair tmp(c, t==KeyPress?true:false); // if keypress true, else false
        if (std::find(fakes.begin(), fakes.end(), tmp) != fakes.end()){
            cout << "one fake occurence skipped: " << tmp.first <<"," << int(tmp.second)<< "\n";
            auto it = std::find(fakes.begin(),fakes.end(),tmp);
            // check that there actually is a 3 in our vector
            if (it != fakes.end()) {
                fakes.erase(it);
            }            
            return;
        }
        // if key is escape, leave (because this generates some problems)
        if(c==9 || repeat)  // avoid repeats for now
            return;

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
        // std::for_each(ctrls.begin(), ctrls.end(), CallMyMethod);
        // std::accumulate(ctrls.begin(), ctrls.end(), false, [c] (bool value, const std::map<int, Key*>::value_type& p)
        //                 { return value ||  (p.second->down &&  p.second->id != c); });
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
                    fakes.push_back(std::make_pair(ctrls[c]->r_id, true));
                    fakes.push_back(std::make_pair(ctrls[c]->r_id, false));
                    XTestFakeKeyEvent(userData->ctrlDisplay, ctrls[c]->r_id,
                                      true, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, ctrls[c]->r_id,
                                      false, CurrentTime);
                }
                else if (ctrls.count(c)!=0 && hit_or_mod(ctrls, c)){
                    fakes.push_back(std::make_pair(c_left_id, true));
                    fakes.push_back(std::make_pair(ctrls[c]->r_id, true));
                    fakes.push_back(std::make_pair(ctrls[c]->r_id, false));
                    fakes.push_back(std::make_pair(c_left_id, false));

                    XTestFakeKeyEvent(userData->ctrlDisplay, c_left_id,
                                      true, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, ctrls[c]->r_id,
                                      true, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, ctrls[c]->r_id,
                                      false, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, c_left_id,
                                      false, CurrentTime);
                } else if (ctrls.count(c)!=0){
                    ctrls[c]->down=true;
                } else if(f_ctrl && ctrls.count(c)==0){  // keys other than fake ctrls
                    cout << "here: "<<c << "\n";
                    fakes.push_back(std::make_pair(backspace_id, true));
                    fakes.push_back(std::make_pair(backspace_id, false));
                    fakes.push_back(std::make_pair(c_left_id, true));
                    // fakes.push_back(std::make_pair(c, true)); // it doesn't not happen
                    fakes.push_back(std::make_pair(c, false));
                    fakes.push_back(std::make_pair(c_left_id, false));

                    XTestFakeKeyEvent(userData->ctrlDisplay, backspace_id,
                                      true, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, backspace_id,
                                      false, CurrentTime);
                    // combo
                    XTestFakeKeyEvent(userData->ctrlDisplay, c_left_id,
                                      true, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, c,
                                      true, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, c,
                                      false, CurrentTime);
                    XTestFakeKeyEvent(userData->ctrlDisplay, c_left_id,
                                      false, CurrentTime);
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
        // if (k_o.down)
        //   cout << "k_o.down = true" << endl;
        // else
        //   cout << "k_o.down = false" << endl;
        // if (k_n.down)
        //   cout << "k_n.down = true" << endl;
        // else
        //   cout << "k_n.down = false" << endl;

        XRecordFreeData(hook);
        // XFlush(userData->ctrlDisplay);
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
