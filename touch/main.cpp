#include "raylib.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <functional>
#include <cstring>

// Plattform-unabhängiger Font-Pfad
#ifdef _WIN32
    const char* FONT_PATH = "C:/Windows/Fonts/arial.ttf";
#elif __linux__
    const char* FONT_PATH = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
#elif __APPLE__
    const char* FONT_PATH = "/System/Library/Fonts/Helvetica.ttc";
#else
    const char* FONT_PATH = nullptr;
#endif

// Standard Fenstergröße
const int DEFAULT_WIDTH = 1024;
const int DEFAULT_HEIGHT = 600;

// Tasten-Definition für Bildschirmtastatur
struct VirtualKey {
    Rectangle rect;
    std::string label;
    std::string value;
    bool isSpecial;
};

struct DrawingPoint {
    Vector2 position;
    float thickness;
    Color color;
    int relativeLine;
    float offsetY;
};

struct TextLine {
    std::string content;
    std::vector<DrawingPoint> drawings;
};

class TextEditor {
private:
    std::vector<TextLine> lines;
    int cursorLine, cursorPos;
    int selectionStartLine, selectionStartPos;
    int selectionEndLine, selectionEndPos;
    bool hasSelection;
    int scrollOffset;
    float scrollPos;
    double cursorTimer;
    bool cursorVisible;
    
    // Zeichenvariablen
    bool isDrawing;
    bool isErasing;
    Vector2 lastDrawPos;
    float currentThickness;
    const float ERASER_SIZE = 15.0f;
    bool eraserMode;
    
    // Fenstervariablen
    int screenWidth, screenHeight;
    int fontSize;
    int lineHeight;
    int textAreaStartX, textAreaStartY;
    int visibleLines;
    int scrollBarWidth;
    int charSpacing;
    
    // Theme mit Shader
    bool darkMode;
    Shader invertShader;
    RenderTexture2D target;
    
    // Zwischenablage
    std::string clipboard;
    
    Font font;
    
    // Datei-Dialog Variablen
    bool showFileDialog;
    bool isSaving;
    std::string fileNameInput;
    char fileNameBuffer[256];
    bool fileDialogActive;
    
    // Window Resize
    bool isResizing;
    Vector2 resizeStart;
    int newScreenWidth, newScreenHeight;
    bool fullscreen;
    
    // UI Buttons
    Rectangle saveButton;
    Rectangle loadButton;
    Rectangle themeButton;
    Rectangle calcButton;
    Rectangle keyboardButton;
    Rectangle eraserModeButton;
    Rectangle brushModeButton;
    bool buttonHovered;
    
    // Brush Size Buttons
    Rectangle brushDecreaseButton;
    Rectangle brushIncreaseButton;
    Rectangle brushSizeDisplay;
    
    // Button Bitmaps
    RenderTexture2D saveIcon;
    RenderTexture2D loadIcon;
    RenderTexture2D themeIcon;
    RenderTexture2D calcIcon;
    RenderTexture2D keyboardIcon;
    RenderTexture2D eraserIcon;
    RenderTexture2D brushIcon;
    bool iconsLoaded;
    
    // Bildschirmtastatur
    bool showVirtualKeyboard;
    bool showSpecialKeyboard;
    std::vector<VirtualKey> virtualKeys;
    bool shiftPressed;
    int keyboardStartY;
    int keyboardHeight;
    
public:
    TextEditor(int width, int height) : screenWidth(width), screenHeight(height) {
        lines.push_back({""});
        cursorLine = cursorPos = 0;
        selectionStartLine = selectionStartPos = 0;
        selectionEndLine = selectionEndPos = 0;
        hasSelection = false;
        scrollOffset = 0;
        scrollPos = 0;
        cursorTimer = 0;
        cursorVisible = true;
        
        isDrawing = false;
        isErasing = false;
        currentThickness = 10.0f;
        darkMode = false;
        isResizing = false;
        fullscreen = false;
        eraserMode = false;
        buttonHovered = false;
        showVirtualKeyboard = false;
        showSpecialKeyboard = false;
        shiftPressed = false;
        iconsLoaded = false;
        keyboardStartY = 0;
        keyboardHeight = 0;
        
        fontSize = 28;
        lineHeight = fontSize + 12;
        charSpacing = 4;
        textAreaStartX = 50;
        textAreaStartY = 30;
        scrollBarWidth = 15;
        
        // UI Buttons
        saveButton = {0, 0, 36, 36};
        loadButton = {0, 0, 36, 36};
        themeButton = {0, 0, 36, 36};
        calcButton = {0, 0, 36, 36};
        keyboardButton = {0, 0, 36, 36};
        eraserModeButton = {0, 0, 36, 36};
        brushModeButton = {0, 0, 36, 36};
        
        // Brush Size Buttons
        brushDecreaseButton = {0, 0, 30, 30};
        brushIncreaseButton = {0, 0, 30, 30};
        brushSizeDisplay = {0, 0, 50, 30};
        
        // Datei-Dialog
        showFileDialog = false;
        isSaving = true;
        fileDialogActive = false;
        memset(fileNameBuffer, 0, sizeof(fileNameBuffer));
        strcpy(fileNameBuffer, "document.rtd");
        fileNameInput = fileNameBuffer;
        
        // Shader für Farbinvertierung
        const char* invertShaderCode = 
            "#version 330\n"
            "in vec2 fragTexCoord;\n"
            "in vec4 fragColor;\n"
            "out vec4 finalColor;\n"
            "uniform sampler2D texture0;\n"
            "void main() {\n"
            "    vec4 texColor = texture(texture0, fragTexCoord);\n"
            "    finalColor = vec4(1.0 - texColor.rgb, texColor.a);\n"
            "}\n";
        
        invertShader = LoadShaderFromMemory(0, invertShaderCode);
        target = LoadRenderTexture(screenWidth, screenHeight);
        
        // Font laden
        font = GetFontDefault();
        if (FONT_PATH) {
            Font loadedFont = LoadFontEx(FONT_PATH, fontSize, 0, 250);
            if (loadedFont.texture.id != 0) {
                font = loadedFont;
            }
        }
        
        CreateButtonIcons();
        UpdateVisibleLines();
    }
    
    ~TextEditor() {
        UnloadFont(font);
        UnloadShader(invertShader);
        UnloadRenderTexture(target);
        
        if (iconsLoaded) {
            UnloadRenderTexture(saveIcon);
            UnloadRenderTexture(loadIcon);
            UnloadRenderTexture(themeIcon);
            UnloadRenderTexture(calcIcon);
            UnloadRenderTexture(keyboardIcon);
            UnloadRenderTexture(eraserIcon);
            UnloadRenderTexture(brushIcon);
        }
    }
    
    void CreateButtonIcons() {
        saveIcon = LoadRenderTexture(36, 36);
        BeginTextureMode(saveIcon);
        ClearBackground(BLANK);
        DrawRectangle(6, 8, 24, 22, BLACK);
        DrawRectangle(10, 8, 16, 8, WHITE);
        DrawRectangle(8, 18, 20, 12, BLACK);
        DrawRectangle(14, 18, 8, 8, WHITE);
        DrawRectangle(16, 26, 4, 4, BLACK);
        EndTextureMode();
        
        loadIcon = LoadRenderTexture(36, 36);
        BeginTextureMode(loadIcon);
        ClearBackground(BLANK);
        DrawRectangle(6, 14, 24, 16, BLACK);
        DrawRectangle(10, 12, 16, 4, BLACK);
        DrawRectangle(8, 10, 20, 4, BLACK);
        DrawTriangle({22, 18}, {26, 22}, {18, 22}, BLACK);
        DrawTriangle({22, 28}, {18, 24}, {26, 24}, BLACK);
        EndTextureMode();
        
        themeIcon = LoadRenderTexture(36, 36);
        BeginTextureMode(themeIcon);
        ClearBackground(BLANK);
        DrawCircle(18, 18, 12, BLACK);
        DrawCircle(14, 14, 8, WHITE);
        for (int i = 0; i < 8; i++) {
            float angle = i * 45 * DEG2RAD;
            int x1 = 18 + cos(angle) * 16;
            int y1 = 18 + sin(angle) * 16;
            int x2 = 18 + cos(angle) * 20;
            int y2 = 18 + sin(angle) * 20;
            DrawLineEx({(float)x1, (float)y1}, {(float)x2, (float)y2}, 2, BLACK);
        }
        EndTextureMode();
        
        calcIcon = LoadRenderTexture(36, 36);
        BeginTextureMode(calcIcon);
        ClearBackground(BLANK);
        DrawRectangle(8, 6, 20, 24, BLACK);
        DrawRectangle(10, 8, 16, 6, WHITE);
        DrawText("=", 12, 20, 10, BLACK);
        DrawText("+", 22, 20, 10, BLACK);
        DrawText("-", 12, 26, 10, BLACK);
        DrawText("*", 22, 26, 10, BLACK);
        EndTextureMode();
        
        keyboardIcon = LoadRenderTexture(36, 36);
        BeginTextureMode(keyboardIcon);
        ClearBackground(BLANK);
        DrawRectangle(6, 14, 24, 12, BLACK);
        DrawRectangle(8, 16, 20, 8, WHITE);
        for (int i = 0; i < 10; i++) {
            DrawRectangle(9 + i * 2, 17, 1, 2, BLACK);
        }
        DrawRectangle(12, 20, 12, 2, BLACK);
        DrawRectangle(12, 22, 12, 2, BLACK);
        EndTextureMode();
        
        eraserIcon = LoadRenderTexture(36, 36);
        BeginTextureMode(eraserIcon);
        ClearBackground(BLANK);
        DrawRectangle(8, 12, 20, 16, BLACK);
        DrawRectangle(10, 14, 16, 12, WHITE);
        DrawLineEx({10, 14}, {26, 26}, 2, BLACK);
        DrawLineEx({10, 26}, {26, 14}, 2, BLACK);
        DrawRectangle(8, 10, 20, 4, BLACK);
        EndTextureMode();
        
        brushIcon = LoadRenderTexture(36, 36);
        BeginTextureMode(brushIcon);
        ClearBackground(BLANK);
        DrawRectangle(8, 16, 4, 14, BLACK);
        DrawCircle(10, 14, 6, BLACK);
        DrawCircle(10, 14, 4, WHITE);
        DrawCircle(10, 12, 2, BLACK);
        EndTextureMode();
        
        iconsLoaded = true;
    }
    
    void DrawButtonIcon(RenderTexture2D icon, Rectangle button) {
        Rectangle sourceRect = {0, 0, 36, 36};
        Rectangle destRect = {button.x, button.y, 36, 36};
        DrawTexturePro(icon.texture, sourceRect, destRect, {0, 0}, 0, WHITE);
    }
    
    void InitKeyboard() {
        int keyWidth = std::max(40, std::min(65, screenWidth / 18));
        int keyHeight = keyWidth;
        int spacing = std::max(4, screenWidth / 150);
        
        int totalWidth = 10 * (keyWidth + spacing) - spacing;
        int startX = (screenWidth - totalWidth) / 2;
        keyboardStartY = screenHeight - keyHeight * 5 - spacing * 5 - 30;
        
        virtualKeys.clear();
        
        // Sonderzeichen Reihe (oben)
        std::vector<std::string> specialRow = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "="};
        for (size_t i = 0; i < specialRow.size(); i++) {
            VirtualKey key;
            key.rect = { (float)(startX + i * (keyWidth + spacing)), (float)keyboardStartY, (float)keyWidth, (float)keyHeight };
            key.label = specialRow[i];
            key.value = specialRow[i];
            key.isSpecial = false;
            virtualKeys.push_back(key);
        }
        
        // Reihe 1
        std::vector<std::string> row1 = {"q", "w", "e", "r", "t", "z", "u", "i", "o", "p"};
        for (size_t i = 0; i < row1.size(); i++) {
            VirtualKey key;
            key.rect = { (float)(startX + i * (keyWidth + spacing)), (float)(keyboardStartY + keyHeight + spacing), (float)keyWidth, (float)keyHeight };
            key.label = row1[i];
            key.value = row1[i];
            key.isSpecial = false;
            virtualKeys.push_back(key);
        }
        
        // Reihe 2
        std::vector<std::string> row2 = {"a", "s", "d", "f", "g", "h", "j", "k", "l"};
        int row2StartX = startX + (keyWidth + spacing) / 2;
        for (size_t i = 0; i < row2.size(); i++) {
            VirtualKey key;
            key.rect = { (float)(row2StartX + i * (keyWidth + spacing)), (float)(keyboardStartY + (keyHeight + spacing) * 2), (float)keyWidth, (float)keyHeight };
            key.label = row2[i];
            key.value = row2[i];
            key.isSpecial = false;
            virtualKeys.push_back(key);
        }
        
        // Reihe 3
        std::vector<std::string> row3 = {"y", "x", "c", "v", "b", "n", "m"};
        int row3StartX = startX + (keyWidth + spacing);
        for (size_t i = 0; i < row3.size(); i++) {
            VirtualKey key;
            key.rect = { (float)(row3StartX + i * (keyWidth + spacing)), (float)(keyboardStartY + (keyHeight + spacing) * 3), (float)keyWidth, (float)keyHeight };
            key.label = row3[i];
            key.value = row3[i];
            key.isSpecial = false;
            virtualKeys.push_back(key);
        }
        
        // Gestapelte Tasten rechts
        int rightSideX = startX + 10 * (keyWidth + spacing) + spacing;
        
        // Backspace
        VirtualKey backspaceKey;
        backspaceKey.rect = { (float)rightSideX, (float)keyboardStartY, (float)(keyWidth + 30), (float)keyHeight };
        backspaceKey.label = "Bcksp";
        backspaceKey.value = "Bcksp";
        backspaceKey.isSpecial = true;
        virtualKeys.push_back(backspaceKey);
        
        // Enter
        VirtualKey enterKey;
        enterKey.rect = { (float)rightSideX, (float)(keyboardStartY + keyHeight + spacing), (float)(keyWidth + 30), (float)keyHeight };
        enterKey.label = "Enter";
        enterKey.value = "\n";
        enterKey.isSpecial = true;
        virtualKeys.push_back(enterKey);
        
        // Shift
        VirtualKey shiftKey;
        shiftKey.rect = { (float)rightSideX, (float)(keyboardStartY + (keyHeight + spacing) * 2), (float)(keyWidth + 30), (float)keyHeight };
        shiftKey.label = "Shift";
        shiftKey.value = "Shift";
        shiftKey.isSpecial = true;
        virtualKeys.push_back(shiftKey);
        
        // Space
        int spaceWidth = totalWidth - (keyWidth + 30);
        VirtualKey spaceKey;
        spaceKey.rect = { (float)(startX + (totalWidth - spaceWidth) / 2), (float)(keyboardStartY + (keyHeight + spacing) * 4), (float)spaceWidth, (float)keyHeight };
        spaceKey.label = "Space";
        spaceKey.value = " ";
        spaceKey.isSpecial = true;
        virtualKeys.push_back(spaceKey);
        
        // Sonderzeichen-Umschalter
        VirtualKey switchKey;
        switchKey.rect = { (float)(startX + totalWidth - (keyWidth + 30)), (float)(keyboardStartY + (keyHeight + spacing) * 4), (float)(keyWidth + 30), (float)keyHeight };
        switchKey.label = "Sym";
        switchKey.value = "Sym";
        switchKey.isSpecial = true;
        virtualKeys.push_back(switchKey);
        
        keyboardHeight = keyHeight * 5 + spacing * 5 + 20;
    }
    
    void InitSpecialKeyboard() {
        int keyWidth = std::max(40, std::min(65, screenWidth / 18));
        int keyHeight = keyWidth;
        int spacing = std::max(4, screenWidth / 150);
        
        int totalWidth = 12 * (keyWidth + spacing) - spacing;
        int startX = (screenWidth - totalWidth) / 2;
        keyboardStartY = screenHeight - keyHeight * 5 - spacing * 5 - 30;
        
        virtualKeys.clear();
        
        // Reihe 1: Mathematische Symbole
        std::vector<std::string> row1 = {"+", "-", "*", "/", "=", "(", ")", "[", "]", "{", "}", "^"};
        for (size_t i = 0; i < row1.size(); i++) {
            VirtualKey key;
            key.rect = { (float)(startX + i * (keyWidth + spacing)), (float)keyboardStartY, (float)keyWidth, (float)keyHeight };
            key.label = row1[i];
            key.value = row1[i];
            key.isSpecial = false;
            virtualKeys.push_back(key);
        }
        
        // Reihe 2: Satzzeichen
        std::vector<std::string> row2 = {"!", "?", ".", ",", ";", ":", "\"", "'", "|", "\\", "@", "#"};
        for (size_t i = 0; i < row2.size(); i++) {
            VirtualKey key;
            key.rect = { (float)(startX + i * (keyWidth + spacing)), (float)(keyboardStartY + keyHeight + spacing), (float)keyWidth, (float)keyHeight };
            key.label = row2[i];
            key.value = row2[i];
            key.isSpecial = false;
            virtualKeys.push_back(key);
        }
        
        // Reihe 3: Währungen und andere
        std::vector<std::string> row3 = {"$", "€", "£", "%", "&", "~", "<", ">", "§", "°", "±", "√"};
        for (size_t i = 0; i < row3.size(); i++) {
            VirtualKey key;
            key.rect = { (float)(startX + i * (keyWidth + spacing)), (float)(keyboardStartY + (keyHeight + spacing) * 2), (float)keyWidth, (float)keyHeight };
            key.label = row3[i];
            key.value = row3[i];
            key.isSpecial = false;
            virtualKeys.push_back(key);
        }
        
        // Gestapelte Tasten rechts
        int rightSideX = startX + 12 * (keyWidth + spacing) + spacing;
        
        // Backspace
        VirtualKey backspaceKey;
        backspaceKey.rect = { (float)rightSideX, (float)keyboardStartY, (float)(keyWidth + 30), (float)keyHeight };
        backspaceKey.label = "Bcksp";
        backspaceKey.value = "Bcksp";
        backspaceKey.isSpecial = true;
        virtualKeys.push_back(backspaceKey);
        
        // Enter
        VirtualKey enterKey;
        enterKey.rect = { (float)rightSideX, (float)(keyboardStartY + keyHeight + spacing), (float)(keyWidth + 30), (float)keyHeight };
        enterKey.label = "Enter";
        enterKey.value = "\n";
        enterKey.isSpecial = true;
        virtualKeys.push_back(enterKey);
        
        // Space
        VirtualKey spaceKey;
        spaceKey.rect = { (float)(startX + totalWidth - (keyWidth + 30) - (keyWidth + 30) - spacing), (float)(keyboardStartY + (keyHeight + spacing) * 3), (float)(keyWidth + 30), (float)keyHeight };
        spaceKey.label = "Space";
        spaceKey.value = " ";
        spaceKey.isSpecial = true;
        virtualKeys.push_back(spaceKey);
        
        // Zurück-Button
        VirtualKey switchBackKey;
        switchBackKey.rect = { (float)(startX + totalWidth - (keyWidth + 30)), (float)(keyboardStartY + (keyHeight + spacing) * 3), (float)(keyWidth + 30), (float)keyHeight };
        switchBackKey.label = "ABC";
        switchBackKey.value = "ABC";
        switchBackKey.isSpecial = true;
        virtualKeys.push_back(switchBackKey);
        
        keyboardHeight = keyHeight * 4 + spacing * 4 + 20;
    }
    
    void UpdateUI() {
        saveButton = {float(screenWidth - 310), 5, 36, 36};
        loadButton = {float(screenWidth - 265), 5, 36, 36};
        themeButton = {float(screenWidth - 220), 5, 36, 36};
        calcButton = {float(screenWidth - 175), 5, 36, 36};
        keyboardButton = {float(screenWidth - 130), 5, 36, 36};
        eraserModeButton = {float(screenWidth - 85), 5, 36, 36};
        brushModeButton = {float(screenWidth - 40), 5, 36, 36};
        
        brushDecreaseButton = {float(screenWidth - 200), 50, 30, 30};
        brushIncreaseButton = {float(screenWidth - 70), 50, 30, 30};
        brushSizeDisplay = {float(screenWidth - 155), 50, 70, 30};
        
        Vector2 mousePos = GetMousePosition();
        
        buttonHovered = CheckCollisionPointRec(mousePos, saveButton) ||
                         CheckCollisionPointRec(mousePos, loadButton) ||
                         CheckCollisionPointRec(mousePos, themeButton) ||
                         CheckCollisionPointRec(mousePos, calcButton) ||
                         CheckCollisionPointRec(mousePos, keyboardButton) ||
                         CheckCollisionPointRec(mousePos, eraserModeButton) ||
                         CheckCollisionPointRec(mousePos, brushModeButton);
        
        if (CheckCollisionPointRec(mousePos, saveButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ShowFileDialog(true);
        }
        
        if (CheckCollisionPointRec(mousePos, loadButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ShowFileDialog(false);
        }
        
        if (CheckCollisionPointRec(mousePos, themeButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            darkMode = !darkMode;
        }
        
        if (CheckCollisionPointRec(mousePos, calcButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ProcessCalculations();
        }
        
        if (CheckCollisionPointRec(mousePos, keyboardButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            showVirtualKeyboard = !showVirtualKeyboard;
            if (showVirtualKeyboard) {
                showSpecialKeyboard = false;
                InitKeyboard();
            }
        }
        
        if (CheckCollisionPointRec(mousePos, eraserModeButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            eraserMode = true;
        }
        
        if (CheckCollisionPointRec(mousePos, brushModeButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            eraserMode = false;
        }
        
        if (CheckCollisionPointRec(mousePos, brushDecreaseButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            currentThickness = std::max(4.0f, currentThickness - 2);
        }
        
        if (CheckCollisionPointRec(mousePos, brushIncreaseButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            currentThickness = std::min(40.0f, currentThickness + 2);
        }
        
        if (showVirtualKeyboard) {
            UpdateVirtualKeyboard();
        }
    }
    
    bool IsMouseInKeyboardArea(Vector2 mousePos) {
        if (!showVirtualKeyboard) return false;
        return (mousePos.y > keyboardStartY - 10);
    }
    
    void UpdateVirtualKeyboard() {
        Vector2 mousePos = GetMousePosition();
        
        for (auto& key : virtualKeys) {
            if (CheckCollisionPointRec(mousePos, key.rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (key.label == "Shift") {
                    shiftPressed = !shiftPressed;
                } else if (key.label == "Space") {
                    InsertChar(' ');
                } else if (key.label == "Bcksp") {
                    HandleBackspace();
                } else if (key.label == "Enter") {
                    HandleEnter();
                } else if (key.label == "Sym") {
                    showSpecialKeyboard = true;
                    InitSpecialKeyboard();
                } else if (key.label == "ABC") {
                    showSpecialKeyboard = false;
                    InitKeyboard();
                } else {
                    std::string charToInsert = key.value;
                    if (!showSpecialKeyboard && shiftPressed) {
                        std::transform(charToInsert.begin(), charToInsert.end(), charToInsert.begin(), ::toupper);
                        shiftPressed = false;
                    }
                    InsertString(charToInsert);
                }
            }
        }
    }
    
    void InsertString(const std::string& text) {
        if (hasSelection) DeleteSelection();
        lines[cursorLine].content.insert(cursorPos, text);
        cursorPos += text.length();
        cursorTimer = 0;
        cursorVisible = true;
        EnsureCursorVisible();
    }
    
    void InsertChar(char c) {
        if (hasSelection) DeleteSelection();
        lines[cursorLine].content.insert(cursorPos, 1, c);
        cursorPos++;
        cursorTimer = 0;
        cursorVisible = true;
        EnsureCursorVisible();
    }
    
    void HandleEnter() {
        if (hasSelection) DeleteSelection();
        std::string remaining = lines[cursorLine].content.substr(cursorPos);
        lines[cursorLine].content = lines[cursorLine].content.substr(0, cursorPos);
        lines.insert(lines.begin() + cursorLine + 1, {remaining});
        cursorLine++;
        cursorPos = 0;
        cursorTimer = 0;
        cursorVisible = true;
        EnsureCursorVisible();
    }
    
    void HandleBackspace() {
        if (hasSelection) { DeleteSelection(); return; }
        if (cursorPos > 0) {
            lines[cursorLine].content.erase(cursorPos - 1, 1);
            cursorPos--;
        } else if (cursorLine > 0) {
            cursorPos = lines[cursorLine - 1].content.length();
            lines[cursorLine - 1].content += lines[cursorLine].content;
            for (auto& d : lines[cursorLine].drawings) lines[cursorLine - 1].drawings.push_back(d);
            lines.erase(lines.begin() + cursorLine);
            cursorLine--;
        }
        cursorTimer = 0;
        cursorVisible = true;
        EnsureCursorVisible();
    }
    
    void HandleDelete() {
        if (hasSelection) { DeleteSelection(); return; }
        if (cursorPos < (int)lines[cursorLine].content.length()) {
            lines[cursorLine].content.erase(cursorPos, 1);
        } else if (cursorLine + 1 < lines.size()) {
            lines[cursorLine].content += lines[cursorLine + 1].content;
            for (auto& d : lines[cursorLine + 1].drawings) lines[cursorLine].drawings.push_back(d);
            lines.erase(lines.begin() + cursorLine + 1);
        }
        cursorTimer = 0;
        cursorVisible = true;
        EnsureCursorVisible();
    }
    
    void EnsureCursorVisible() {
        if (cursorLine < scrollOffset) {
            scrollOffset = cursorLine;
        } else if (cursorLine >= scrollOffset + visibleLines) {
            scrollOffset = cursorLine - visibleLines + 1;
        }
        
        int maxScroll = std::max(0, (int)lines.size() - visibleLines);
        scrollOffset = std::max(0, std::min(maxScroll, scrollOffset));
        if (maxScroll > 0) {
            scrollPos = (float)scrollOffset / maxScroll;
        }
    }
    
    void UpdateVisibleLines() {
        int keyboardSpace = 0;
        if (showVirtualKeyboard) {
            keyboardSpace = keyboardHeight + 30;
        }
        visibleLines = (screenHeight - textAreaStartY - keyboardSpace) / lineHeight;
        if (visibleLines < 1) visibleLines = 1;
    }
    
    void UpdateWindow() {
        Vector2 mousePos = GetMousePosition();
        Rectangle resizeGrip = {float(screenWidth - 20), float(screenHeight - 20), 20, 20};
        
        if (CheckCollisionPointRec(mousePos, resizeGrip)) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE);
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, resizeGrip)) {
            isResizing = true;
            resizeStart = mousePos;
            newScreenWidth = screenWidth;
            newScreenHeight = screenHeight;
        }
        
        if (isResizing && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int deltaX = mousePos.x - resizeStart.x;
            int deltaY = mousePos.y - resizeStart.y;
            newScreenWidth = std::max(800, screenWidth + deltaX);
            newScreenHeight = std::max(600, screenHeight + deltaY);
            resizeStart = mousePos;
            screenWidth = newScreenWidth;
            screenHeight = newScreenHeight;
            SetWindowSize(screenWidth, screenHeight);
            
            UnloadRenderTexture(target);
            target = LoadRenderTexture(screenWidth, screenHeight);
            
            UpdateVisibleLines();
            if (showVirtualKeyboard) {
                if (showSpecialKeyboard) {
                    InitSpecialKeyboard();
                } else {
                    InitKeyboard();
                }
            }
        }
        
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            isResizing = false;
        }
        
        if (IsKeyPressed(KEY_F11)) {
            fullscreen = !fullscreen;
            ToggleFullscreen();
            screenWidth = GetScreenWidth();
            screenHeight = GetScreenHeight();
            
            UnloadRenderTexture(target);
            target = LoadRenderTexture(screenWidth, screenHeight);
            
            UpdateVisibleLines();
            if (showVirtualKeyboard) {
                if (showSpecialKeyboard) {
                    InitSpecialKeyboard();
                } else {
                    InitKeyboard();
                }
            }
        }
        
        if (IsWindowResized()) {
            screenWidth = GetScreenWidth();
            screenHeight = GetScreenHeight();
            UnloadRenderTexture(target);
            target = LoadRenderTexture(screenWidth, screenHeight);
            UpdateVisibleLines();
            if (showVirtualKeyboard) {
                if (showSpecialKeyboard) {
                    InitSpecialKeyboard();
                } else {
                    InitKeyboard();
                }
            }
        }
    }
    
    void ShowFileDialog(bool save) {
        showFileDialog = true;
        isSaving = save;
        fileDialogActive = true;
        memset(fileNameBuffer, 0, sizeof(fileNameBuffer));
        strcpy(fileNameBuffer, "document.rtd");
        fileNameInput = fileNameBuffer;
    }
    
    void HandleFileDialog() {
        if (!showFileDialog) return;
        
        int dialogWidth = 400;
        int dialogHeight = 150;
        int dialogX = screenWidth/2 - dialogWidth/2;
        int dialogY = screenHeight/2 - dialogHeight/2;
        
        DrawRectangle(dialogX, dialogY, dialogWidth, dialogHeight, LIGHTGRAY);
        DrawRectangleLines(dialogX, dialogY, dialogWidth, dialogHeight, DARKGRAY);
        
        const char* title = isSaving ? "Save File" : "Load File";
        DrawText(title, dialogX + 20, dialogY + 20, 20, DARKGRAY);
        
        Rectangle inputRect = {float(dialogX + 20), float(dialogY + 60), float(dialogWidth - 40), 30};
        DrawRectangleRec(inputRect, WHITE);
        DrawRectangleLinesEx(inputRect, 2, BLUE);
        
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 125 && fileNameInput.length() < 255) {
                fileNameInput += (char)key;
            }
            key = GetCharPressed();
        }
        
        if (IsKeyPressed(KEY_BACKSPACE) && fileNameInput.length() > 0) {
            fileNameInput.pop_back();
        }
        
        if (IsKeyPressed(KEY_ENTER)) {
            if (isSaving) {
                SaveFile(fileNameInput);
            } else {
                LoadFile(fileNameInput);
            }
            showFileDialog = false;
            fileDialogActive = false;
        }
        
        if (IsKeyPressed(KEY_ESCAPE)) {
            showFileDialog = false;
            fileDialogActive = false;
        }
        
        DrawText(fileNameInput.c_str(), dialogX + 25, dialogY + 65, 18, BLACK);
        
        if (((int)GetTime() * 2) % 2 == 0) {
            float cursorX = dialogX + 25 + MeasureText(fileNameInput.substr(0, fileNameInput.length()).c_str(), 18);
            DrawRectangle(cursorX, dialogY + 63, 2, 20, BLACK);
        }
        
        Rectangle saveButtonRect = {float(dialogX + dialogWidth - 130), float(dialogY + dialogHeight - 40), 50, 30};
        Rectangle cancelButtonRect = {float(dialogX + dialogWidth - 70), float(dialogY + dialogHeight - 40), 60, 30};
        
        Vector2 mousePos = GetMousePosition();
        
        DrawRectangleRec(saveButtonRect, CheckCollisionPointRec(mousePos, saveButtonRect) ? BLUE : DARKGRAY);
        DrawText(isSaving ? "Save" : "Load", saveButtonRect.x + 8, saveButtonRect.y + 8, 14, WHITE);
        
        DrawRectangleRec(cancelButtonRect, CheckCollisionPointRec(mousePos, cancelButtonRect) ? RED : DARKGRAY);
        DrawText("Cancel", cancelButtonRect.x + 8, cancelButtonRect.y + 8, 14, WHITE);
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mousePos, saveButtonRect)) {
                if (isSaving) {
                    SaveFile(fileNameInput);
                } else {
                    LoadFile(fileNameInput);
                }
                showFileDialog = false;
                fileDialogActive = false;
            }
            if (CheckCollisionPointRec(mousePos, cancelButtonRect)) {
                showFileDialog = false;
                fileDialogActive = false;
            }
        }
    }
    
    float GetCharWidth(char c) {
        return MeasureTextEx(font, std::string(1, c).c_str(), fontSize, charSpacing).x;
    }
    
    float MeasureTextWidth(const std::string& text) {
        float width = 0;
        for (char c : text) {
            width += GetCharWidth(c);
        }
        return width;
    }
    
    int GetCursorX(int line, int pos) {
        if (line >= lines.size()) return textAreaStartX;
        if (pos < 0) pos = 0;
        if (pos > (int)lines[line].content.length()) pos = lines[line].content.length();
        
        const std::string& text = lines[line].content;
        float width = 0;
        for (int i = 0; i < pos; i++) {
            width += GetCharWidth(text[i]);
        }
        return textAreaStartX + (int)width;
    }
    
    int FindPosAtX(int line, int mouseX) {
        if (line >= lines.size()) return 0;
        
        const std::string& text = lines[line].content;
        float currentWidth = 0;
        
        for (int i = 0; i <= (int)text.length(); i++) {
            if (mouseX < textAreaStartX + currentWidth) {
                return i;
            }
            if (i < (int)text.length()) {
                currentWidth += GetCharWidth(text[i]);
            }
        }
        return text.length();
    }
    
    double evaluateExpression(const std::string& expr) {
        size_t pos = 0;
        return parseExpression(expr, pos);
    }
    
    double parseExpression(const std::string& expr, size_t& pos) {
        double result = parseTerm(expr, pos);
        while (pos < expr.length()) {
            if (expr[pos] == '+') { pos++; result += parseTerm(expr, pos); }
            else if (expr[pos] == '-') { pos++; result -= parseTerm(expr, pos); }
            else break;
        }
        return result;
    }
    
    double parseTerm(const std::string& expr, size_t& pos) {
        double result = parseFactor(expr, pos);
        while (pos < expr.length()) {
            if (expr[pos] == '*') { pos++; result *= parseFactor(expr, pos); }
            else if (expr[pos] == '/') { pos++; double d = parseFactor(expr, pos); if (d != 0) result /= d; }
            else break;
        }
        return result;
    }
    
    double parseFactor(const std::string& expr, size_t& pos) {
        while (pos < expr.length() && isspace(expr[pos])) pos++;
        if (pos >= expr.length()) return 0;
        
        if (pos + 4 <= expr.length() && expr.substr(pos, 4) == "sqrt") {
            pos += 4;
            if (expr[pos] == '(') { pos++; double arg = parseExpression(expr, pos); if (expr[pos] == ')') pos++; return sqrt(arg); }
        }
        else if (pos + 3 <= expr.length() && expr.substr(pos, 3) == "exp") {
            pos += 3;
            if (expr[pos] == '(') { pos++; double exp = parseExpression(expr, pos); if (expr[pos] == ',') { pos++; double base = parseExpression(expr, pos); if (expr[pos] == ')') pos++; return pow(base, exp); } }
        }
        
        if (expr[pos] == '(') { pos++; double result = parseExpression(expr, pos); if (expr[pos] == ')') pos++; return result; }
        
        std::string numStr;
        while (pos < expr.length() && (isdigit(expr[pos]) || expr[pos] == '.')) { numStr += expr[pos]; pos++; }
        
        if (!numStr.empty()) return std::stod(numStr);
        return 0;
    }
    
    void ProcessCalculations() {
        for (auto& line : lines) {
            std::string& l = line.content;
            std::string newLine;
            size_t lastPos = 0;
            size_t pos = 0;
            
            while ((pos = l.find('[', pos)) != std::string::npos) {
                size_t end = l.find(']', pos + 1);
                if (end == std::string::npos) break;
                
                newLine += l.substr(lastPos, pos - lastPos);
                std::string expr = l.substr(pos + 1, end - pos - 1);
                
                try {
                    double result = evaluateExpression(expr);
                    std::string resStr = std::to_string(result);
                    resStr.erase(resStr.find_last_not_of('0') + 1, std::string::npos);
                    if (resStr.back() == '.') resStr.pop_back();
                    newLine += "[" + expr + "=" + resStr + "]";
                } catch (...) { newLine += "[" + expr + "=ERROR]"; }
                
                lastPos = end + 1;
                pos = end + 1;
            }
            newLine += l.substr(lastPos);
            l = newLine;
        }
    }
    
    void AddDrawingPoint(Vector2 pos, float thickness, Color color) {
        int lineIndex = -1;
        float offsetY = 0;
        
        for (int i = 0; i < lines.size(); i++) {
            int lineY = textAreaStartY + (i - scrollOffset) * lineHeight;
            if (pos.y >= lineY && pos.y < lineY + lineHeight) {
                lineIndex = i;
                offsetY = pos.y - lineY;
                break;
            }
        }
        
        if (lineIndex == -1) { 
            lineIndex = cursorLine; 
            offsetY = pos.y - (textAreaStartY + (cursorLine - scrollOffset) * lineHeight); 
        }
        if (lineIndex >= 0 && lineIndex < lines.size()) { 
            lines[lineIndex].drawings.push_back({pos, thickness, color, lineIndex, offsetY}); 
        }
    }
    
    void EraseAtPosition(Vector2 pos) {
        float eraserSize = ERASER_SIZE;
        for (auto& line : lines) {
            line.drawings.erase(std::remove_if(line.drawings.begin(), line.drawings.end(),
                [pos, eraserSize](const DrawingPoint& p) { 
                    float dx = pos.x - p.position.x;
                    float dy = pos.y - p.position.y;
                    float dist = sqrt(dx*dx + dy*dy);
                    return dist < eraserSize; 
                }), line.drawings.end());
        }
    }
    
    void SaveFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file.write((char*)&darkMode, sizeof(bool));
            int count = lines.size(); file.write((char*)&count, sizeof(int));
            for (const auto& line : lines) {
                int len = line.content.length(); file.write((char*)&len, sizeof(int));
                file.write(line.content.c_str(), len);
                int dc = line.drawings.size(); file.write((char*)&dc, sizeof(int));
                for (const auto& d : line.drawings) {
                    file.write((char*)&d.position, sizeof(Vector2));
                    file.write((char*)&d.thickness, sizeof(float));
                    file.write((char*)&d.color, sizeof(Color));
                    file.write((char*)&d.relativeLine, sizeof(int));
                    file.write((char*)&d.offsetY, sizeof(float));
                }
            }
            file.close();
        }
    }
    
    void LoadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (file.is_open()) {
            lines.clear();
            file.read((char*)&darkMode, sizeof(bool));
            int count; file.read((char*)&count, sizeof(int));
            for (int i = 0; i < count; i++) {
                TextLine line;
                int len; file.read((char*)&len, sizeof(int));
                line.content.resize(len);
                file.read(&line.content[0], len);
                int dc; file.read((char*)&dc, sizeof(int));
                for (int j = 0; j < dc; j++) {
                    DrawingPoint d;
                    file.read((char*)&d.position, sizeof(Vector2));
                    file.read((char*)&d.thickness, sizeof(float));
                    file.read((char*)&d.color, sizeof(Color));
                    file.read((char*)&d.relativeLine, sizeof(int));
                    file.read((char*)&d.offsetY, sizeof(float));
                    line.drawings.push_back(d);
                }
                lines.push_back(line);
            }
            file.close();
            cursorLine = cursorPos = 0; scrollOffset = 0; hasSelection = false;
        }
    }
    
    void DeleteSelection() {
        if (!hasSelection) return;
        
        std::string before = lines[selectionStartLine].content.substr(0, selectionStartPos);
        std::string after = lines[selectionEndLine].content.substr(selectionEndPos);
        lines[selectionStartLine].content = before + after;
        
        for (int l = selectionEndLine; l > selectionStartLine; l--) {
            for (auto& d : lines[l].drawings) lines[selectionStartLine].drawings.push_back(d);
            lines.erase(lines.begin() + l);
        }
        cursorLine = selectionStartLine;
        cursorPos = selectionStartPos;
        hasSelection = false;
        EnsureCursorVisible();
    }
    
    std::string GetSelectedText() {
        if (!hasSelection) return "";
        std::string sel;
        for (int l = selectionStartLine; l <= selectionEndLine; l++) {
            const std::string& txt = lines[l].content;
            int start = (l == selectionStartLine) ? selectionStartPos : 0;
            int end = (l == selectionEndLine) ? selectionEndPos : txt.length();
            sel += txt.substr(start, end - start);
            if (l < selectionEndLine) sel += "\n";
        }
        return sel;
    }
    
    void DrawContent() {
        // Zeichnungen
        for (int i = 0; i < lines.size(); i++) {
            int y = textAreaStartY + (i - scrollOffset) * lineHeight;
            if (y + lineHeight < textAreaStartY || y > screenHeight) continue;
            for (const auto& d : lines[i].drawings) {
                DrawCircleV({d.position.x, y + d.offsetY}, d.thickness / 2, d.color);
            }
        }
        
        // Text
        for (int i = 0; i < visibleLines && (scrollOffset + i) < lines.size(); i++) {
            int line = scrollOffset + i;
            int y = textAreaStartY + i * lineHeight;
            DrawText(TextFormat("%d", line + 1), 10, y, fontSize - 8, LIGHTGRAY);
            
            const std::string& txt = lines[line].content;
            float x = textAreaStartX;
            
            for (int j = 0; j < (int)txt.length(); j++) {
                char c = txt[j];
                float w = GetCharWidth(c);
                bool sel = hasSelection && line >= selectionStartLine && line <= selectionEndLine &&
                    j >= (line == selectionStartLine ? selectionStartPos : 0) &&
                    j < (line == selectionEndLine ? selectionEndPos : (int)txt.length());
                
                if (sel) { 
                    DrawRectangle(x, y - 2, w, lineHeight, ColorAlpha(BLUE, 0.5f)); 
                    DrawTextEx(font, std::string(1, c).c_str(), {x, float(y)}, fontSize, charSpacing, WHITE); 
                } else {
                    DrawTextEx(font, std::string(1, c).c_str(), {x, float(y)}, fontSize, charSpacing, BLACK); 
                }
                x += w;
            }
            
            if (cursorLine == line && cursorVisible && !hasSelection) {
                int cursorX = GetCursorX(line, cursorPos);
                DrawRectangle(cursorX, y, 2, fontSize, BLACK);
            }
        }
        
        // Buttons
        if (iconsLoaded) {
            DrawRectangle(saveButton.x - 5, 0, 340, 46, ColorAlpha(LIGHTGRAY, 0.5f));
            
            DrawButtonIcon(saveIcon, saveButton);
            DrawButtonIcon(loadIcon, loadButton);
            DrawButtonIcon(themeIcon, themeButton);
            DrawButtonIcon(calcIcon, calcButton);
            DrawButtonIcon(keyboardIcon, keyboardButton);
            
            if (eraserMode) {
                DrawRectangleRec(eraserModeButton, ColorAlpha(RED, 0.3f));
            }
            if (!eraserMode) {
                DrawRectangleRec(brushModeButton, ColorAlpha(GREEN, 0.3f));
            }
            DrawButtonIcon(eraserIcon, eraserModeButton);
            DrawButtonIcon(brushIcon, brushModeButton);
        }
        
        // Brush Size Controls
        DrawRectangleRec(brushDecreaseButton, LIGHTGRAY);
        DrawRectangleLinesEx(brushDecreaseButton, 1, DARKGRAY);
        DrawText("<", brushDecreaseButton.x + 10, brushDecreaseButton.y + 5, 18, BLACK);
        
        DrawRectangleRec(brushSizeDisplay, LIGHTGRAY);
        DrawRectangleLinesEx(brushSizeDisplay, 1, DARKGRAY);
        DrawText(TextFormat("%.0f", currentThickness), brushSizeDisplay.x + 20, brushSizeDisplay.y + 5, 16, BLACK);
        
        DrawRectangleRec(brushIncreaseButton, LIGHTGRAY);
        DrawRectangleLinesEx(brushIncreaseButton, 1, DARKGRAY);
        DrawText(">", brushIncreaseButton.x + 10, brushIncreaseButton.y + 5, 18, BLACK);
        
        // Virtuelle Tastatur
        if (showVirtualKeyboard && keyboardStartY > 0) {
            // Hintergrund für gesamte Tastatur
            DrawRectangle(0, keyboardStartY - 10, screenWidth, keyboardHeight + 20, ColorAlpha(DARKGRAY, 0.9f));
            DrawRectangleLines(0, keyboardStartY - 10, screenWidth, keyboardHeight + 20, GRAY);
            
            for (const auto& key : virtualKeys) {
                Color keyColor = LIGHTGRAY;
                if (key.label == "Shift" && shiftPressed) keyColor = DARKGREEN;
                if (key.label == "Sym" || key.label == "ABC") keyColor = BLUE;
                DrawRectangleRec(key.rect, keyColor);
                DrawRectangleLinesEx(key.rect, 1, DARKGRAY);
                
                float textX = key.rect.x + key.rect.width/2 - MeasureText(key.label.c_str(), 16)/2;
                float textY = key.rect.y + key.rect.height/2 - 8;
                DrawText(key.label.c_str(), textX, textY, 16, BLACK);
            }
        }
        
        // Scrollbar
        Rectangle sb = {float(screenWidth - scrollBarWidth - 10), float(textAreaStartY), float(scrollBarWidth), float(visibleLines * lineHeight)};
        DrawRectangleRec(sb, LIGHTGRAY);
        int maxScroll = std::max(0, (int)lines.size() - visibleLines);
        if (maxScroll > 0) {
            float h = sb.height * ((float)visibleLines / lines.size());
            float y = sb.y + scrollPos * (sb.height - h);
            DrawRectangle(screenWidth - scrollBarWidth - 10, y, scrollBarWidth, h, DARKGRAY);
        }
        
        // Resize Grip
        DrawTriangle(
            {float(screenWidth - 15), float(screenHeight - 5)},
            {float(screenWidth - 5), float(screenHeight - 15)},
            {float(screenWidth - 5), float(screenHeight - 5)},
            GRAY
        );
    }
    
    void Update() {
        if (fileDialogActive) return;
        
        UpdateUI();
        UpdateWindow();
        
        UpdateVisibleLines();
        cursorTimer += GetFrameTime();
        if (cursorTimer >= 0.5) { cursorTimer = 0; cursorVisible = !cursorVisible; }
        
        Vector2 mp = GetMousePosition();
        
        bool inKeyboardArea = IsMouseInKeyboardArea(mp);
        
        // Radieren
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || (eraserMode && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
            if (!isErasing) { isErasing = true; EraseAtPosition(mp); } 
            else { EraseAtPosition(mp); }
        } else { isErasing = false; }
        
        // Zeichnen
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !eraserMode && !isErasing && !buttonHovered && !inKeyboardArea) {
            if (!isDrawing) { 
                isDrawing = true; 
                AddDrawingPoint(mp, currentThickness, BLACK); 
                lastDrawPos = mp; 
            } else {
                AddDrawingPoint(mp, currentThickness, BLACK);
                float dist = sqrt(pow(mp.x - lastDrawPos.x, 2) + pow(mp.y - lastDrawPos.y, 2));
                if (dist > currentThickness / 2) {
                    int steps = (int)(dist / (currentThickness / 2)) + 1;
                    for (int i = 1; i < steps; i++) {
                        float t = (float)i / steps;
                        Vector2 ip = {lastDrawPos.x + t * (mp.x - lastDrawPos.x), lastDrawPos.y + t * (mp.y - lastDrawPos.y)};
                        AddDrawingPoint(ip, currentThickness, BLACK);
                    }
                }
                lastDrawPos = mp;
            }
        } else { isDrawing = false; }
        
        // Physikalische Tastatur
        if (!showVirtualKeyboard) {
            int key = GetCharPressed();
            while (key > 0) {
                if (hasSelection) DeleteSelection();
                if (key >= 32 && key <= 125) {
                    lines[cursorLine].content.insert(cursorPos, 1, (char)key);
                    cursorPos++;
                    cursorTimer = 0; cursorVisible = true;
                    EnsureCursorVisible();
                }
                key = GetCharPressed();
            }
            
            if (IsKeyPressed(KEY_ENTER)) {
                HandleEnter();
            }
            
            if (IsKeyPressed(KEY_BACKSPACE)) { 
                HandleBackspace(); 
            }
            
            if (IsKeyPressed(KEY_DELETE)) { 
                HandleDelete(); 
            }
        }
        
        // Pfeiltasten
        if (IsKeyPressed(KEY_LEFT)) {
            if (cursorPos > 0) cursorPos--; 
            else if (cursorLine > 0) { 
                cursorLine--; 
                cursorPos = lines[cursorLine].content.length(); 
            }
            EnsureCursorVisible();
            cursorTimer = 0; cursorVisible = true;
        }
        
        if (IsKeyPressed(KEY_RIGHT)) {
            if (cursorPos < (int)lines[cursorLine].content.length()) cursorPos++; 
            else if (cursorLine + 1 < lines.size()) { 
                cursorLine++; 
                cursorPos = 0; 
            }
            EnsureCursorVisible();
            cursorTimer = 0; cursorVisible = true;
        }
        
        if (IsKeyPressed(KEY_UP)) {
            if (cursorLine > 0) { 
                cursorLine--; 
                cursorPos = std::min(cursorPos, (int)lines[cursorLine].content.length()); 
            }
            EnsureCursorVisible();
            cursorTimer = 0; cursorVisible = true;
        }
        
        if (IsKeyPressed(KEY_DOWN)) {
            if (cursorLine + 1 < lines.size()) { 
                cursorLine++; 
                cursorPos = std::min(cursorPos, (int)lines[cursorLine].content.length()); 
            }
            EnsureCursorVisible();
            cursorTimer = 0; cursorVisible = true;
        }
        
        // Textauswahl mit RECHTSKLICK
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && mp.x >= textAreaStartX && mp.y >= textAreaStartY && !inKeyboardArea) {
            int line = (int)(mp.y - textAreaStartY) / lineHeight + scrollOffset;
            if (line >= 0 && line < lines.size()) {
                int pos = FindPosAtX(line, mp.x);
                selectionStartLine = selectionEndLine = line;
                selectionStartPos = selectionEndPos = pos;
                hasSelection = true;
                cursorLine = line; cursorPos = pos;
                EnsureCursorVisible();
            }
        }
        
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && hasSelection && mp.x >= textAreaStartX && mp.y >= textAreaStartY && !inKeyboardArea) {
            int line = (int)(mp.y - textAreaStartY) / lineHeight + scrollOffset;
            if (line >= 0 && line < lines.size()) {
                int pos = FindPosAtX(line, mp.x);
                selectionEndLine = line; selectionEndPos = pos;
                cursorLine = line; cursorPos = pos;
                if (selectionStartLine > selectionEndLine || (selectionStartLine == selectionEndLine && selectionStartPos > selectionEndPos)) {
                    std::swap(selectionStartLine, selectionEndLine);
                    std::swap(selectionStartPos, selectionEndPos);
                }
                EnsureCursorVisible();
            }
        }
        
        // Scrollen
        int wheel = GetMouseWheelMove();
        if (wheel != 0 && !isDrawing && !isErasing && !inKeyboardArea) {
            scrollOffset -= wheel * 2;
            int max = std::max(0, (int)lines.size() - visibleLines);
            scrollOffset = std::max(0, std::min(max, scrollOffset));
            if (max > 0) scrollPos = (float)scrollOffset / max;
        }
    }
    
    void Draw() {
        if (darkMode) {
            BeginTextureMode(target);
            ClearBackground(RAYWHITE);
            DrawContent();
            EndTextureMode();
            
            BeginDrawing();
            ClearBackground(BLACK);
            BeginShaderMode(invertShader);
            DrawTextureRec(target.texture, {0, 0, (float)screenWidth, (float)-screenHeight}, {0, 0}, WHITE);
            EndShaderMode();
            if (showFileDialog) HandleFileDialog();
            EndDrawing();
        } else {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawContent();
            if (showFileDialog) HandleFileDialog();
            EndDrawing();
        }
        if (showFileDialog && !fileDialogActive) showFileDialog = false;
    }
};

int main() {
    InitWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Text Editor");
    SetTargetFPS(60);
    
    TextEditor editor(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    while (!WindowShouldClose()) {
        editor.Update();
        editor.Draw();
    }
    
    CloseWindow();
    return 0;
}
