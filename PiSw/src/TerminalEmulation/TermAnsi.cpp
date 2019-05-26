// Bus Raider
// Rob Dobson 2019
// Portions Copyright (c) 2017 Rob King

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "TermAnsi.h"

#define MAX(x, y) (((size_t)(x) > (size_t)(y)) ? (size_t)(x) : (size_t)(y))
#define MIN(x, y) (((size_t)(x) < (size_t)(y)) ? (size_t)(x) : (size_t)(y))

#define P0(x) (_ansiParams[x])
#define P1(x) (_ansiParams[x] ? _ansiParams[x] : 1)

// Log string
const char *FromTermAnsi = "TermAnsi";

void TermAnsi::init(uint32_t cols, uint32_t rows)
{
    TermEmu::init(cols, rows);
    reset();
}

void TermAnsi::reset()
{
    TermEmu::reset();
    _cursor._off = false;
    _ansiParamsNum = 0;
    _arg = 0;
    _vtIgnored = false;
    for (int i = 0; i < MAX_ANSI_TERMINAL_LINES; i++)
        _lineDirty[i] = true;
    _curAttrs._backColour = TermChar::TERM_DEFAULT_BACK_COLOUR;
    _curAttrs._foreColour = TermChar::TERM_DEFAULT_FORE_COLOUR;
    _curAttrs._charCode = ' ';
    _curAttrs._attribs = 0;
    resetparser();
}

void TermAnsi::putChar(uint32_t ch)
{
    if (handleAnsiChar(ch))
        return;
    writeCharAtCurs(ch);
    _cursor._updated = true;
}

bool TermAnsi::handleAnsiChar(uint8_t ch)
{
    char cs[] = {(char)ch, 0};
    #define ON(S, C, A) if (_vtState == (S) && strchr(C, ch)){ A; return true;}
    #define DO(S, C, A) ON(S, C, consumearg(); if (!_vtIgnored) {A;} fixcursor(); resetparser(););

    DO(S_NUL, "\x07",       vtCallback(TMT_MSG_BELL, NULL))
    DO(S_NUL, "\x08",       if (_cursor._col) _cursor._col--)
    DO(S_NUL, "\x09",       while (++_cursor._col < _cols - 1 && _tabLine[_cursor._col]._charCode != '*'))
    DO(S_NUL, "\x0a",       _cursor._row < _rows - 1? (void)_cursor._row++ : scrollUp(0, 1))
    DO(S_NUL, "\x0d",       _cursor._col = 0)
    ON(S_NUL, "\x1b",       _vtState = S_ESC)
    ON(S_ESC, "\x1b",       _vtState = S_ESC)
    DO(S_ESC, "H",          _tabLine[_cursor._col]._charCode = '*')
    DO(S_ESC, "7",          _oldAttrs = _curAttrs; _oldCursor = _cursor)
    DO(S_ESC, "8",          _curAttrs = _oldAttrs; _cursor = _oldCursor)
    ON(S_ESC, "+*()",       _vtIgnored = true; _vtState = S_ARG)
    DO(S_ESC, "c",          reset())
    ON(S_ESC, "[",          _vtState = S_ARG)
    ON(S_ARG, "\x1b",       _vtState = S_ESC)
    ON(S_ARG, ";",          consumearg())
    ON(S_ARG, "?",          (void)0)
    ON(S_ARG, "0123456789", _arg = _arg * 10 + atoi(cs))
    DO(S_ARG, "A",          _cursor._row = MAX(_cursor._row - P1(0), 0))
    DO(S_ARG, "B",          _cursor._row = MIN(_cursor._row + P1(0), _rows - 1))
    DO(S_ARG, "C",          _cursor._col = MIN(_cursor._col + P1(0), _cols - 1))
    DO(S_ARG, "D",          _cursor._col = MIN(_cursor._col - P1(0), _cursor._col))
    DO(S_ARG, "E",          _cursor._col = 0; _cursor._row = MIN(_cursor._row + P1(0), _rows - 1))
    DO(S_ARG, "F",          _cursor._col = 0; _cursor._row = MAX(_cursor._row - P1(0), 0))
    DO(S_ARG, "G",          _cursor._col = MIN(P1(0) - 1, _cols - 1))
    DO(S_ARG, "d",          _cursor._row = MIN(P1(0) - 1, _rows - 1))
    DO(S_ARG, "Hf",         _cursor._row = P1(0) - 1; _cursor._col = P1(1) - 1)
    DO(S_ARG, "I",          while (++_cursor._col < _cols - 1 && _tabLine[_cursor._col]._charCode != '*'))
    DO(S_ARG, "J",          ed())
    DO(S_ARG, "K",          el())
    DO(S_ARG, "L",          scrollDown(_cursor._row, P1(0)))
    DO(S_ARG, "M",          scrollUp(_cursor._row, P1(0)))
    DO(S_ARG, "P",          dch())
    DO(S_ARG, "S",          scrollUp(0, P1(0)))
    DO(S_ARG, "T",          scrollDown(0, P1(0)))
    DO(S_ARG, "X",          clearline(_cursor._row, _cursor._col, P1(0)))
    DO(S_ARG, "Z",          while (_cursor._col && _tabLine[--_cursor._col]._charCode != '*'))
    DO(S_ARG, "b",          rep());
    DO(S_ARG, "c",          vtCallback(TMT_MSG_ANSWER, "\033[?6c"))
    DO(S_ARG, "g",          if (P0(0) == 3) { for (uint32_t i = 0; i < _cols; i++) _tabLine[i].clear(); })
    DO(S_ARG, "m",          sgr())
    DO(S_ARG, "n",          if (P0(0) == 6) dsr())
    DO(S_ARG, "h",          if (P0(0) == 25) vtCallback(TMT_MSG_CURSOR, "t"))
    DO(S_ARG, "i",          (void)0)
    DO(S_ARG, "l",          if (P0(0) == 25) vtCallback(TMT_MSG_CURSOR, "f"))
    DO(S_ARG, "s",          _oldAttrs = _curAttrs; _oldCursor = _cursor)
    DO(S_ARG, "u",          _curAttrs = _oldAttrs; _cursor = _oldCursor)
    DO(S_ARG, "@",          ich())

    resetparser();
    return false;
}

void TermAnsi::fixcursor()
{
    _cursor._row = MIN(_cursor._row, _rows - 1);
    _cursor._col = MIN(_cursor._col, _cols - 1);
}

void TermAnsi::consumearg()
{
    if (_ansiParamsNum < MAX_ANSI_PARAMS)
        _ansiParams[_ansiParamsNum++] = _arg;
    _arg = 0;
}

void TermAnsi::resetparser()
{
    memset(_ansiParams, 0, sizeof(_ansiParams));
    _vtState = S_NUL;
    _ansiParamsNum = 0;
    _arg = 0;
    _vtIgnored = false;
}

void TermAnsi::dirtylines(size_t start, size_t end)
{
    _cursor._updated = true;
    for (size_t i = start; i < end; i++)
        _lineDirty[i] = true;
}

void TermAnsi::clearline(size_t lineIdx, size_t startCol, size_t endCol)
{
    _cursor._updated = true;
    if (lineIdx >= _rows)
        lineIdx = _rows-1;
    _lineDirty[lineIdx] = true;
    for (size_t i = startCol; i < endCol && i < _cols; i++)
    {
        _pCharBuffer[lineIdx * _cols + i]._attribs = DEFAULT_ATTRIBS;
        _pCharBuffer[lineIdx * _cols + i]._foreColour = TermChar::TERM_DEFAULT_FORE_COLOUR;
        _pCharBuffer[lineIdx * _cols + i]._backColour = TermChar::TERM_DEFAULT_BACK_COLOUR;
        _pCharBuffer[lineIdx * _cols + i]._charCode = ' ';
    }
}

void TermAnsi::clearlines(size_t rowIdx, size_t numRows)
{
    for (size_t i = rowIdx; i < rowIdx + numRows && i < _rows; i++)
        clearline(i, 0, _cols);
}

void TermAnsi::writeCharAtCurs(int ch)
{
    _pCharBuffer[_cursor._row * _cols + _cursor._col]._charCode = ch;
    _pCharBuffer[_cursor._row * _cols + _cursor._col]._backColour = _curAttrs._backColour;
    _pCharBuffer[_cursor._row * _cols + _cursor._col]._foreColour = _curAttrs._foreColour;
    _pCharBuffer[_cursor._row * _cols + _cursor._col]._attribs = _curAttrs._attribs;
    _cursor._updated = true;

    if (_cursor._col < _cols - 1)
        _cursor._col++;
    else
    {
        _cursor._col = 0;
        _cursor._row++;
    }

    if (_cursor._row >= _rows)
    {
        _cursor._row = _rows - 1;
        scrollUp(0, 1);
    }
}

void TermAnsi::scrollUp(size_t startRow, size_t n)
{
    n = MIN(n, _rows - 1 - startRow);

    if (!_pCharBuffer)
        return;
    
    if (n){
        TermChar* pBuf = new TermChar[n*_cols];

        memcpy(pBuf, _pCharBuffer + startRow * _cols, n * sizeof(TermChar) * _cols);
        memmove(_pCharBuffer + startRow * _cols, _pCharBuffer + (startRow + n) * _cols,
               (_rows - n - startRow) * sizeof(TermChar) * _cols);
        memcpy(_pCharBuffer + (_rows - n) * _cols,
               pBuf, n * sizeof(TermChar) * _cols);
        
        delete [] pBuf;

        clearlines(_rows - n, n);
        dirtylines(startRow, _rows);
    }
}

void TermAnsi::scrollDown(size_t startRow, size_t n)
{
    n = MIN(n, _rows - 1 - startRow);

    if (!_pCharBuffer)
        return;
    
    if (n){
        TermChar* pBuf = new TermChar[n*_cols];

        memcpy(pBuf, _pCharBuffer + (_rows-n) * _cols, n * sizeof(TermChar) * _cols);
        memmove(_pCharBuffer + (startRow+n) * _cols, _pCharBuffer + startRow * _cols,
               (_rows - n - startRow) * sizeof(TermChar) * _cols);
        memcpy(_pCharBuffer + startRow * _cols,
               pBuf, n * sizeof(TermChar) * _cols);
        
        delete [] pBuf;

        clearlines(startRow, n);
        dirtylines(startRow, _rows);
    }
}

void TermAnsi::ed()
{
    size_t b = 0;
    size_t e = _rows;

    switch (P0(0))
    {
    case 0:
        b = _cursor._row + 1;
        clearline(_cursor._row, _cursor._col, _cols);
        break;
    case 1:
        e = _cursor._row - 1;
        clearline(_cursor._row, 0, _cursor._col);
        break;
    case 2: /* use defaults */
        break;
    default: /* do nothing   */
        return;
    }

    clearlines(b, e - b);
}

void TermAnsi::el()
{
    switch (P0(0))
    {
    case 0:
        clearline(_cursor._row, _cursor._col, _cols);
        break;
    case 1:
        clearline(_cursor._row, 0, MIN(_cursor._col + 1, _cols - 1));
        break;
    case 2:
        clearline(_cursor._row, 0, _cols);
        break;
    }
}

void TermAnsi::dch()
{
    size_t n = P1(0); /* XXX use MAX */
    if (n > _cols - _cursor._col)
        n = _cols - _cursor._col;

    memmove(_pCharBuffer + (_cursor._row * _cols) + _cursor._col, _pCharBuffer + (_cursor._row * _cols) + _cursor._col + n,
            (_cols - _cursor._col - n) * sizeof(TermChar) * _cols);

    clearline(_cursor._row, _cols - _cursor._col - n, _cols);
}

void TermAnsi::ich()
{
    size_t n = P1(0); /* XXX use MAX */
    if (n > _cols - _cursor._col - 1)
        n = _cols - _cursor._col - 1;

    memmove(_pCharBuffer + (_cursor._row * _cols) + _cursor._col + n, _pCharBuffer + (_cursor._row * _cols) + _cursor._col,
            MIN(_cols - 1 - _cursor._col, (_cols - _cursor._col - n - 1)) * sizeof(TermChar) * _cols);
    clearline(_cursor._row, _cursor._col, n);
}

void TermAnsi::rep()
{
    if (!_cursor._col)
        return;
    uint8_t repCh = _pCharBuffer[(_cursor._row*_cols) + _cursor._col - 1]._charCode;
    for (size_t i = 0; i < P1(0); i++)
        writeCharAtCurs(repCh);
}

void TermAnsi::sgr()
{
    #define FGBG(c) *(P0(i) < 40 ? &_curAttrs._foreColour : &_curAttrs._foreColour) = c
    tmt_attrs attrs;
    attrs.attrs = _curAttrs._attribs;
    for (size_t i = 0; i < _ansiParamsNum; i++)
    {
        switch (P0(i))
        {
        case 0:
            attrs.attrs = DEFAULT_ATTRIBS;
            break;
        case 1:
        case 22:

            attrs.bold = P0(0) < 20;
            break;
        case 2:
        case 23:
            attrs.dim = P0(0) < 20;
            break;
        case 4:
        case 24:
            attrs.underline = P0(0) < 20;
            break;
        case 5:
        case 25:
            attrs.blink = P0(0) < 20;
            break;
        case 7:
        case 27:
            attrs.reverse = P0(0) < 20;
            break;
        case 8:
        case 28:
            attrs.invisible = P0(0) < 20;
            break;
        case 10:
        case 11:
            // vt->acs = P0(0) > 10;
            break;
        case 30:
        case 40:
            FGBG(TMT_COLOR_BLACK);
            break;
        case 31:
        case 41:
            FGBG(TMT_COLOR_RED);
            break;
        case 32:
        case 42:
            FGBG(TMT_COLOR_GREEN);
            break;
        case 33:
        case 43:
            FGBG(TMT_COLOR_YELLOW);
            break;
        case 34:
        case 44:
            FGBG(TMT_COLOR_BLUE);
            break;
        case 35:
        case 45:
            FGBG(TMT_COLOR_MAGENTA);
            break;
        case 36:
        case 46:
            FGBG(TMT_COLOR_CYAN);
            break;
        case 37:
        case 47:
            FGBG(TMT_COLOR_WHITE);
            break;
        case 39:
        case 49:
            FGBG(TMT_COLOR_DEFAULT);
            break;
        }
    }
    _curAttrs._attribs = attrs.attrs;
}

void TermAnsi::dsr()
{
    // char r[BUF_MAX + 1] = {0};
    // snprintf(r, BUF_MAX, "\033[%zd;%zdR", _cursor._row, _cursor._col);
    // vtCallback(TMT_MSG_ANSWER, (const char *)r);
}