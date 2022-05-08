#include "editor.h"
#include <ncurses.h>
#include <cstdio>
#include <bitset>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* node */

Node* Node::nth(s32 n) {
    return (n < 0) ? this : (this->next) ? this->next->nth(n - 1) : nullptr;
}

/* buffer */

Buffer::Buffer(u8* values, u32 len) {
    this->data = new Node;
    this->data->value = len;

    Node* prev = this->data;
    for (u32 i = 0; i < len; i++) {
        prev->next = new Node;
        prev->next->value = *(values + i);
        prev = prev->next;
    }
}

Buffer::~Buffer() {
    for (Node *head = this->data, *temp; head != nullptr; head = temp) {
        temp = head->next;
        delete head;
    }
}

u8 Buffer::revise(Node* node, u8 value) {
    u8 oldValue = node->value;
    node->value = value & 0xFF;
    return oldValue;
}

u8 Buffer::insert(Node* prev, u8 value) {
    Node* temp = new Node;
    temp->value = value & 0xFF;
    temp->next = prev->next;
    prev->next = temp;

    this->data->value++;
    return 0;
}

u8 Buffer::remove(Node* prev) {
    Node* temp = prev->next;
    prev->next = temp->next;

    u8 oldValue = temp->value;
    delete temp;
    this->data->value--;
    return oldValue;
}

/* editor */

Editor::Editor(string fileName): fileName(fileName) {
    FILE* file = fopen(fileName.c_str(), "ab+");
    if (file == nullptr) {
        printf("invalid binary file: %s\n", fileName.c_str());
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    u32 fileSize = (u32)ftell(file);
    fseek(file, 0, SEEK_SET);

    u8* array = new u8 [fileSize];
    for (u32 i = 0; i < fileSize; i++)
        fread(array + i, 1, 1, file);
    
    this->buffer = new Buffer(array, fileSize);
    delete [] array;
    fclose(file);
}

Editor::~Editor() {
    delete this->buffer;
    endwin();
}

void Editor::revise(u32 idx, u8 value) {
    assert (idx < this->buffer->data->value);
    u8 oldValue = this->buffer->revise(this->buffer->data->nth(idx), value);
    this->undoStack.push(Command {
        .type = REVISE,
        .idx = idx,
        .oldValue = oldValue,
        .newValue = value
    });
    this->redoStack = {};
}

void Editor::insert(u32 idx, u8 value) {
    assert (idx <= this->buffer->data->value);
    this->buffer->insert(this->buffer->data->nth(idx - 1), value);
    this->undoStack.push(Command {
        .type = INSERT,
        .idx = idx,
        .oldValue = 0,
        .newValue = value
    });
    this->redoStack = {};
}

void Editor::remove(u32 idx) {
    assert (idx < this->buffer->data->value);
    u8 oldValue = this->buffer->remove(this->buffer->data->nth(idx - 1));
    this->undoStack.push(Command {
        .type = REMOVE,
        .idx = idx,
        .oldValue = oldValue,
        .newValue = 0
    });
    this->redoStack = {};
}

void Editor::undo(s32 n) {
    if (this->undoStack.empty() || n == 0) return;

    Command command = this->undoStack.top();
    switch (command.type) {
    case REVISE:
        this->buffer->revise(this->buffer->data->nth(command.idx), command.oldValue);
        break;
    case INSERT:
        this->buffer->remove(this->buffer->data->nth(command.idx - 1));
        break;
    case REMOVE:
        this->buffer->insert(this->buffer->data->nth(command.idx - 1), command.oldValue);
        break;
    }

    this->undoStack.pop();
    this->redoStack.push(command);
    this->undo(n - 1);
}

void Editor::redo(s32 n) {
    if (this->redoStack.empty() || n == 0) return;

    Command command = this->redoStack.top();
    switch (command.type) {
    case REVISE:
        this->buffer->revise(this->buffer->data->nth(command.idx), command.newValue);
        break;
    case INSERT:
        this->buffer->insert(this->buffer->data->nth(command.idx - 1), command.newValue);
        break;
    case REMOVE:
        this->buffer->remove(this->buffer->data->nth(command.idx - 1));
        break;
    }

    this->redoStack.pop();
    this->undoStack.push(command);
    this->redo(n - 1);
}

void Editor::save() {
    FILE* file = fopen(this->fileName.c_str(), "wb");
    for (Node* head = this->buffer->data->nth(0); head != nullptr; head = head->next)
        fwrite(&head->value, 1, 1, file);
    fclose(file);
}

void Editor::quit() {
    this->isQuitting = true;
}

void Editor::run() {
    // initialize ternimal window and ncurses
    printf("\e[8;24;80t");
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, true);
    // print file name as title
    mvprintw(0, 0, this->fileName.c_str());
    refresh();
    // enter main loop
    this->handleEvent();
}

void printCommandStack(WINDOW* win, stack<Command> commands, u32 maxLine) {
    for (u32 i = 0; !commands.empty() && i < maxLine; i++) {
        Command command = commands.top();
        switch (command.type) {
        case REVISE:
            mvwprintw(win, i, 0, "%08X:  revise %02X >> %02X", command.idx, command.oldValue, command.newValue);
            break;
        case INSERT:
            mvwprintw(win, i, 0, "%08X:  insert %02X", command.idx, command.newValue);
            break;
        case REMOVE:
            mvwprintw(win, i, 0, "%08X:  remove %02X", command.idx, command.oldValue);
            break;
        }
        commands.pop();
    }
}

void Editor::updateHistoryWin() {
    // render outer window
    WINDOW* undoOuter = newwin(11, 30, 1, 0);
    box(undoOuter, 0, 0); mvwprintw(undoOuter, 0, 2, " Undo stack ");
    wrefresh(undoOuter);
    WINDOW* redoOuter = newwin(11, 30, 12, 0);
    box(redoOuter, 0, 0); mvwprintw(redoOuter, 0, 2, " Redo stack ");
    wrefresh(redoOuter);
    // render inner window
    WINDOW* undoInner = newwin(9, 26, 2, 2);
    printCommandStack(undoInner, this->undoStack, 9);
    wrefresh(undoInner);
    WINDOW* redoInner = newwin(9, 26, 13, 2);
    printCommandStack(redoInner, this->redoStack, 9);
    wrefresh(redoInner);
    // clean up windows
    delwin(undoOuter);
    delwin(redoOuter);
    delwin(undoInner);
}

void Editor::updateMainWin() {
    // render outer window
    WINDOW* hexOuter = newwin(18, 38, 1, 30);
    box(hexOuter, 0, 0); mvwprintw(hexOuter, 0, 2, " Hex ");
    wrefresh(hexOuter);
    WINDOW* asciiOuter = newwin(18, 12, 1, 68);
    box(asciiOuter, 0, 0); mvwprintw(asciiOuter, 0, 2, " Ascii ");
    wrefresh(asciiOuter);
    // render inner window
    WINDOW* hexInner = newwin(16, 34, 2, 32);
    WINDOW* asciiInner = newwin(16, 8, 2, 70);

    Node* head = this->buffer->data->nth(0);
    for (u32 i = 0; head != nullptr; head = head->next, i++) {
        if (i % 8 == 0x0) {
            mvwprintw(hexInner, i >> 3, 0, "%08X: ", i);
            wmove(asciiInner, i >> 3, 0);
        }
        wprintw(hexInner, " ");
        if (this->cursor.mainWinFixed && this->cursor.editPos == i) {
            wattron(hexInner, A_STANDOUT);
            this->cursor.value = head->value;
        }
        wprintw(hexInner, "%02X", head->value);
        wattroff(hexInner, A_STANDOUT);
        wattroff(hexInner, A_BLINK);
        char ch = (char)head->value;
        wprintw(asciiInner, "%c", (ch >= 32 && ch <= 126) ? ch : '.');
    }
    
    wrefresh(hexInner);
    wrefresh(asciiInner);

    // clean up windows
    delwin(hexOuter);
    delwin(asciiOuter);
    delwin(hexInner);
    delwin(hexInner);
}

void Editor::updateDetailWin() {
    // render outer window
    WINDOW* outer = newwin(4, 50, 19, 30);
    box(outer, 0, 0); mvwprintw(outer, 0, 2, " Detail ");
    wrefresh(outer);
    // render inner window
    WINDOW* inner = newwin(2, 46, 20, 32);
    mvwprintw(inner, 0, 0, "binary: %8s", bitset<8>(this->cursor.value).to_string().c_str());
    mvwprintw(inner, 1, 0, "octal : %03o", this->cursor.value);
    mvwprintw(inner, 0, 23, "unsigned decimal: %d", this->cursor.value);
    mvwprintw(inner, 1, 23, "signed   decimal: %d", (s8)this->cursor.value);
    wrefresh(inner);
    // clean up windows
    delwin(outer);
    delwin(inner);
}

void Editor::handleEvent() {
    this->updateHistoryWin();
    this->updateMainWin();
    this->updateDetailWin();
    if (this->isQuitting) return;
    // render buttons
    enum EVENTS {UNDO, REDO, EDIT, SAVE, QUIT};
    string buttonName[] = {"undo", "redo", "edit", "save", "quit"};
    for (u32 i = 0; i < 5; i++) {
        if (this->cursor.buttonIdx == i)
            attron(A_STANDOUT);
        mvprintw(23, i * 16 + 8, buttonName[i].c_str());
        attroff(A_STANDOUT);
    }
    // handle events
    switch (getch()) {
    case KEY_LEFT: this->cursor.buttonIdx--; break;
    case KEY_RIGHT: this->cursor.buttonIdx++; break;
    case '\n':
        switch (this->cursor.buttonIdx) {
        case UNDO: this->undo(); break;
        case REDO: this->redo(); break;
        case EDIT: this->cursor.mainWinFixed = true; this->handleMainWinEvent(); break;
        case SAVE: this->save(); break;
        case QUIT: this->isQuitting = true; break;
        }
        break;
    }
    // adjust button idx
    this->cursor.buttonIdx = MAX(this->cursor.buttonIdx, 0);
    this->cursor.buttonIdx = MIN(this->cursor.buttonIdx, 4);
    // infinite loop
    this->handleEvent();
}

void Editor::handleMainWinEvent() {
    this->updateHistoryWin();
    this->updateMainWin();
    this->updateDetailWin();
    if (!this->cursor.mainWinFixed) return;
    // handle events
    switch (u32 ch = getch()) {
    case KEY_LEFT: this->cursor.editPos -= 1; break;
    case KEY_UP: this->cursor.editPos -= 8; break;
    case KEY_RIGHT: this->cursor.editPos += 1; break;
    case KEY_DOWN: this->cursor.editPos += 8; break;
    // space
    case ' ': this->insert(this->cursor.editPos, 0); break;
    // backspace
    case KEY_BACKSPACE:
    case '\b': case 127: this->remove(this->cursor.editPos--); break;
    case '\n': this->cursor.mainWinFixed = false; break;
    default:
        if (48 <= ch && ch <= 57) // 0 ~ 9
            this->revise(this->cursor.editPos, (this->cursor.value << 4) | (ch - 48));
        else if (97 < ch && ch <= 102) // a ~ f
            this->revise(this->cursor.editPos, (this->cursor.value << 4) | (ch - 87));
    }
    // adjust cursor pos
    this->cursor.editPos = MAX(this->cursor.editPos, 0);
    this->cursor.editPos = MIN(this->cursor.editPos, this->buffer->data->value);
    if (this->cursor.editPos == this->buffer->data->value)
        this->insert(this->buffer->data->value, 0);
    // infinite loop
    this->handleMainWinEvent();
}
