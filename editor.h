#include <string>
#include <stack>

using namespace std;

typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;

enum EDITOR_COMMAND {REVISE, INSERT, REMOVE};

struct Command {
    EDITOR_COMMAND type;
    u32 idx;
    u8 oldValue, newValue;
};

struct Node {
    // attributes
    u32 value;
    Node* next;
    // interfaces
    Node* nth(s32 n);
};

class Buffer {
public:
    // initializer
    Buffer(u8* values, u32 len);
    ~Buffer();
    // attributes
    Node* data;
    // interfaces
    u8 revise(Node* node, u8 value);
    u8 insert(Node* prev, u8 value);
    u8 remove(Node* prev);
};

class Editor {
public:
    // initializer
    Editor(string fileName);
    ~Editor();
    // attributes
    string fileName;
    // interfaces
    void revise(u32 idx, u8 value);
    void insert(u32 idx, u8 value);
    void remove(u32 idx);
    void undo(s32 n=1);
    void redo(s32 n=1);
    void save();
    void quit();
    void run();
private:
    // attributes
    struct Cursor {
        bool mainWinFixed;
        s32 editPos, buttonIdx;
        u32 value;
    } cursor = {
        .mainWinFixed = false,
        .editPos = -1,
        .buttonIdx = 0,
        .value = 0
    };
    Buffer* buffer;
    stack<Command> undoStack, redoStack;
    bool isQuitting = false;
    // interfaces
    void updateHistoryWin();
    void updateMainWin();
    void updateDetailWin();
    void handleEvent();
    void handleMainWinEvent();
};
