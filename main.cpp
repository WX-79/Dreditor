#include "raylib.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>
#include <functional>

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

// UTF-8 Sonderzeichen Mapping
struct SpecialChar {
    const char* utf8;
    int raylibKey;
    bool needsShift;
};

// Struktur für gezeichnete Punkte/Kreise
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
    
    // Tasten-Wiederholung
    double lastKeyTime;
    double keyRepeatDelay;
    double keyRepeatInterval;
    int lastKeyPressed;
    bool keyRepeatActive;
    
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
    
    // Sonderzeichen Unterstützung
    bool altGrPressed;
    
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
        altGrPressed = false;
        
        fontSize = 28;
        lineHeight = fontSize + 12;
        charSpacing = 4;
        textAreaStartX = 50;
        textAreaStartY = 30;
        scrollBarWidth = 15;
        
        // Tasten-Wiederholung (schnell)
        lastKeyTime = 0;
        keyRepeatDelay = 0.2;   // 200ms Verzögerung
        keyRepeatInterval = 0.02;  // 20ms Intervall (50x pro Sekunde)
        lastKeyPressed = -1;
        keyRepeatActive = false;
        
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
        
        // Render Target für Shader-Effekt
        target = LoadRenderTexture(screenWidth, screenHeight);
        
        // System-Font laden (mit UTF-8 Unterstützung)
        font = GetFontDefault();
        if (FONT_PATH) {
            Font loadedFont = LoadFontEx(FONT_PATH, fontSize, 0, 250);
            if (loadedFont.texture.id != 0) {
                font = loadedFont;
                std::cout << "Loaded system font with UTF-8 support" << std::endl;
            } else {
                std::cout << "Using default font" << std::endl;
            }
        }
        
        UpdateVisibleLines();
    }
    
    ~TextEditor() {
        UnloadFont(font);
        UnloadShader(invertShader);
        UnloadRenderTexture(target);
    }
    
    void UpdateVisibleLines() {
        visibleLines = (screenHeight - textAreaStartY - 50) / lineHeight;
        if (visibleLines < 1) visibleLines = 1;
    }
    
    void UpdateWindow() {
        // Fenster-Resize mit Maus
        Vector2 mousePos = GetMousePosition();
        Rectangle resizeGrip = {float(screenWidth - 20), float(screenHeight - 20), 20, 20};
        
        // Prüfe ob Maus über Resize-Grip ist
        if (CheckCollisionPointRec(mousePos, resizeGrip)) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE);
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
        
        // Resize starten
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, resizeGrip)) {
            isResizing = true;
            resizeStart = mousePos;
            newScreenWidth = screenWidth;
            newScreenHeight = screenHeight;
        }
        
        // Resize durchführen
        if (isResizing && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int deltaX = mousePos.x - resizeStart.x;
            int deltaY = mousePos.y - resizeStart.y;
            newScreenWidth = std::max(800, screenWidth + deltaX);
            newScreenHeight = std::max(600, screenHeight + deltaY);
            resizeStart = mousePos;
            screenWidth = newScreenWidth;
            screenHeight = newScreenHeight;
            SetWindowSize(screenWidth, screenHeight);
            
            // Render Texture neu erstellen für neue Größe
            UnloadRenderTexture(target);
            target = LoadRenderTexture(screenWidth, screenHeight);
            
            UpdateVisibleLines();
        }
        
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            isResizing = false;
        }
        
        // Fullscreen toggle mit F11
        if (IsKeyPressed(KEY_F11)) {
            fullscreen = !fullscreen;
            ToggleFullscreen();
            if (fullscreen) {
                screenWidth = GetScreenWidth();
                screenHeight = GetScreenHeight();
            } else {
                screenWidth = 1200;
                screenHeight = 800;
                SetWindowSize(screenWidth, screenHeight);
            }
            
            // Render Texture neu erstellen für neue Größe
            UnloadRenderTexture(target);
            target = LoadRenderTexture(screenWidth, screenHeight);
            
            UpdateVisibleLines();
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
        
        int dialogWidth = 500;
        int dialogHeight = 200;
        int dialogX = screenWidth/2 - dialogWidth/2;
        int dialogY = screenHeight/2 - dialogHeight/2;
        
        DrawRectangle(dialogX, dialogY, dialogWidth, dialogHeight, LIGHTGRAY);
        DrawRectangleLines(dialogX, dialogY, dialogWidth, dialogHeight, DARKGRAY);
        
        const char* title = isSaving ? "Save File" : "Load File";
        DrawText(title, dialogX + 20, dialogY + 20, 24, DARKGRAY);
        DrawText("Filename:", dialogX + 20, dialogY + 70, 18, DARKGRAY);
        
        Rectangle inputRect = {float(dialogX + 20), float(dialogY + 95), float(dialogWidth - 40), 30};
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
        
        DrawText(fileNameInput.c_str(), dialogX + 25, dialogY + 100, 18, BLACK);
        
        if (((int)GetTime() * 2) % 2 == 0) {
            float cursorX = dialogX + 25 + MeasureText(fileNameInput.substr(0, fileNameInput.length()).c_str(), 18);
            DrawRectangle(cursorX, dialogY + 98, 2, 20, BLACK);
        }
        
        Rectangle saveButton = {float(dialogX + dialogWidth - 160), float(dialogY + dialogHeight - 50), 60, 30};
        Rectangle cancelButton = {float(dialogX + dialogWidth - 90), float(dialogY + dialogHeight - 50), 70, 30};
        
        Vector2 mousePos = GetMousePosition();
        
        DrawRectangleRec(saveButton, CheckCollisionPointRec(mousePos, saveButton) ? BLUE : DARKGRAY);
        DrawText(isSaving ? "Save" : "Load", saveButton.x + 12, saveButton.y + 8, 16, WHITE);
        
        DrawRectangleRec(cancelButton, CheckCollisionPointRec(mousePos, cancelButton) ? RED : DARKGRAY);
        DrawText("Cancel", cancelButton.x + 12, cancelButton.y + 8, 16, WHITE);
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mousePos, saveButton)) {
                if (isSaving) {
                    SaveFile(fileNameInput);
                } else {
                    LoadFile(fileNameInput);
                }
                showFileDialog = false;
                fileDialogActive = false;
            }
            if (CheckCollisionPointRec(mousePos, cancelButton)) {
                showFileDialog = false;
                fileDialogActive = false;
            }
        }
    }
    
    float MeasureTextWidth(const std::string& text) {
        float width = 0;
        // Für UTF-8 Zeichen müssen wir jedes Zeichen korrekt messen
        for (size_t i = 0; i < text.length(); ) {
            // UTF-8 Zeichenlänge bestimmen
            int charLen = 1;
            if ((text[i] & 0xF8) == 0xF0) charLen = 4;
            else if ((text[i] & 0xF0) == 0xE0) charLen = 3;
            else if ((text[i] & 0xE0) == 0xC0) charLen = 2;
            
            std::string utf8Char = text.substr(i, charLen);
            width += MeasureTextEx(font, utf8Char.c_str(), fontSize, charSpacing).x;
            i += charLen;
        }
        return width;
    }
    
    float GetCharWidth(const std::string& utf8Char) {
        return MeasureTextEx(font, utf8Char.c_str(), fontSize, charSpacing).x;
    }
    
    int GetCursorX(int line, int pos) {
        if (line >= lines.size()) return textAreaStartX;
        if (pos < 0) pos = 0;
        
        std::string text = lines[line].content;
        // Position in Bytes umwandeln (UTF-8 safe)
        int bytePos = 0;
        int charCount = 0;
        while (bytePos < text.length() && charCount < pos) {
            if ((text[bytePos] & 0xF8) == 0xF0) bytePos += 4;
            else if ((text[bytePos] & 0xF0) == 0xE0) bytePos += 3;
            else if ((text[bytePos] & 0xE0) == 0xC0) bytePos += 2;
            else bytePos += 1;
            charCount++;
        }
        
        std::string textBeforeCursor = text.substr(0, bytePos);
        return textAreaStartX + (int)MeasureTextWidth(textBeforeCursor);
    }
    
    int FindPosAtX(int line, int mouseX) {
        if (line >= lines.size()) return 0;
        
        const std::string& text = lines[line].content;
        float currentWidth = 0;
        int charCount = 0;
        size_t bytePos = 0;
        
        while (bytePos < text.length()) {
            // Prüfe ob Maus vor diesem Zeichen ist
            if (mouseX < textAreaStartX + currentWidth) {
                return charCount;
            }
            
            // UTF-8 Zeichenlänge
            int charLen = 1;
            if ((text[bytePos] & 0xF8) == 0xF0) charLen = 4;
            else if ((text[bytePos] & 0xF0) == 0xE0) charLen = 3;
            else if ((text[bytePos] & 0xE0) == 0xC0) charLen = 2;
            
            std::string utf8Char = text.substr(bytePos, charLen);
            currentWidth += GetCharWidth(utf8Char);
            bytePos += charLen;
            charCount++;
        }
        
        return charCount;
    }
    
    // Sonderzeichen Eingabe
    void HandleSpecialChars() {
        // AltGr Erkennung (Rechts Alt)
        altGrPressed = IsKeyDown(KEY_RIGHT_ALT);
        
        // Deutsches Tastaturlayout Sonderzeichen
        if (altGrPressed) {
            // AltGr + Q = @
            if (IsKeyPressed(KEY_Q)) InsertChar("@");
            // AltGr + E = €
            else if (IsKeyPressed(KEY_E)) InsertChar("€");
            // AltGr + 2 = ²
            else if (IsKeyPressed(KEY_TWO)) InsertChar("²");
            // AltGr + 3 = ³
            else if (IsKeyPressed(KEY_THREE)) InsertChar("³");
            // AltGr + 7 = {
            else if (IsKeyPressed(KEY_SEVEN)) InsertChar("{");
            // AltGr + 8 = [
            else if (IsKeyPressed(KEY_EIGHT)) InsertChar("[");
            // AltGr + 9 = ]
            else if (IsKeyPressed(KEY_NINE)) InsertChar("]");
            // AltGr + 0 = }
            else if (IsKeyPressed(KEY_ZERO)) InsertChar("}");
            // AltGr + - = \
            else if (IsKeyPressed(KEY_MINUS)) InsertChar("\\");
            // AltGr + < = |
            else if (IsKeyPressed(KEY_COMMA)) InsertChar("|");
        } else {
            // Normale Sonderzeichen mit Shift
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                if (IsKeyPressed(KEY_SEMICOLON)) InsertChar(":");
                else if (IsKeyPressed(KEY_PERIOD)) InsertChar(";");
                else if (IsKeyPressed(KEY_COMMA)) InsertChar("<");
                else if (IsKeyPressed(KEY_SLASH)) InsertChar("?");
                else if (IsKeyPressed(KEY_GRAVE)) InsertChar("~");
                else if (IsKeyPressed(KEY_ONE)) InsertChar("!");
                else if (IsKeyPressed(KEY_TWO)) InsertChar("\"");
                else if (IsKeyPressed(KEY_THREE)) InsertChar("§");
                else if (IsKeyPressed(KEY_FOUR)) InsertChar("$");
                else if (IsKeyPressed(KEY_FIVE)) InsertChar("%");
                else if (IsKeyPressed(KEY_SIX)) InsertChar("&");
                else if (IsKeyPressed(KEY_SEVEN)) InsertChar("/");
                else if (IsKeyPressed(KEY_EIGHT)) InsertChar("(");
                else if (IsKeyPressed(KEY_NINE)) InsertChar(")");
                else if (IsKeyPressed(KEY_ZERO)) InsertChar("=");
                else if (IsKeyPressed(KEY_MINUS)) InsertChar("_");
                else if (IsKeyPressed(KEY_EQUAL)) InsertChar("+");
                else if (IsKeyPressed(KEY_LEFT_BRACKET)) InsertChar("{");
                else if (IsKeyPressed(KEY_RIGHT_BRACKET)) InsertChar("}");
                else if (IsKeyPressed(KEY_BACKSLASH)) InsertChar("|");
            } else {
                // Umlaute und ß (kein Shift)
                if (IsKeyPressed(KEY_APOSTROPHE)) InsertChar("ä");
                else if (IsKeyPressed(KEY_LEFT_BRACKET)) InsertChar("ü");
                else if (IsKeyPressed(KEY_RIGHT_BRACKET)) InsertChar("ö");
                else if (IsKeyPressed(KEY_MINUS)) InsertChar("ß");
                else if (IsKeyPressed(KEY_SEMICOLON)) InsertChar("ö");
                else if (IsKeyPressed(KEY_PERIOD)) InsertChar(".");
                else if (IsKeyPressed(KEY_COMMA)) InsertChar(",");
                else if (IsKeyPressed(KEY_SLASH)) InsertChar("-");
                else if (IsKeyPressed(KEY_GRAVE)) InsertChar("^");
                else if (IsKeyPressed(KEY_ONE)) InsertChar("1");
                else if (IsKeyPressed(KEY_TWO)) InsertChar("2");
                else if (IsKeyPressed(KEY_THREE)) InsertChar("3");
                else if (IsKeyPressed(KEY_FOUR)) InsertChar("4");
                else if (IsKeyPressed(KEY_FIVE)) InsertChar("5");
                else if (IsKeyPressed(KEY_SIX)) InsertChar("6");
                else if (IsKeyPressed(KEY_SEVEN)) InsertChar("7");
                else if (IsKeyPressed(KEY_EIGHT)) InsertChar("8");
                else if (IsKeyPressed(KEY_NINE)) InsertChar("9");
                else if (IsKeyPressed(KEY_ZERO)) InsertChar("0");
                else if (IsKeyPressed(KEY_EQUAL)) InsertChar("´");
                else if (IsKeyPressed(KEY_BACKSLASH)) InsertChar("#");
            }
        }
    }
    
    void InsertChar(const std::string& utf8Char) {
        if (hasSelection) DeleteSelection();
        lines[cursorLine].content.insert(cursorPos, utf8Char);
        cursorPos++;
        cursorTimer = 0;
        cursorVisible = true;
    }
    
    // Rechner Implementierung
    double evaluateExpression(const std::string& expr) {
        size_t pos = 0;
        return parseExpression(expr, pos);
    }
    
    double parseExpression(const std::string& expr, size_t& pos) {
        double result = parseTerm(expr, pos);
        while (pos < expr.length()) {
            if (expr[pos] == '+') {
                pos++;
                result += parseTerm(expr, pos);
            } else if (expr[pos] == '-') {
                pos++;
                result -= parseTerm(expr, pos);
            } else {
                break;
            }
        }
        return result;
    }
    
    double parseTerm(const std::string& expr, size_t& pos) {
        double result = parseFactor(expr, pos);
        while (pos < expr.length()) {
            if (expr[pos] == '*') {
                pos++;
                result *= parseFactor(expr, pos);
            } else if (expr[pos] == '/') {
                pos++;
                double divisor = parseFactor(expr, pos);
                if (divisor != 0) result /= divisor;
            } else {
                break;
            }
        }
        return result;
    }
    
    double parseFactor(const std::string& expr, size_t& pos) {
        while (pos < expr.length() && isspace(expr[pos])) pos++;
        if (pos >= expr.length()) return 0;
        
        if (pos + 4 <= expr.length() && expr.substr(pos, 4) == "sqrt") {
            pos += 4;
            if (expr[pos] == '(') {
                pos++;
                double arg = parseExpression(expr, pos);
                if (expr[pos] == ')') pos++;
                return sqrt(arg);
            }
        }
        else if (pos + 3 <= expr.length() && expr.substr(pos, 3) == "exp") {
            pos += 3;
            if (expr[pos] == '(') {
                pos++;
                double exponent = parseExpression(expr, pos);
                if (expr[pos] == ',') {
                    pos++;
                    double base = parseExpression(expr, pos);
                    if (expr[pos] == ')') pos++;
                    return pow(base, exponent);
                }
            }
        }
        else if (pos + 3 <= expr.length() && expr.substr(pos, 3) == "sin") {
            pos += 3;
            if (expr[pos] == '(') {
                pos++;
                double arg = parseExpression(expr, pos);
                if (expr[pos] == ')') pos++;
                return sin(arg);
            }
        }
        else if (pos + 3 <= expr.length() && expr.substr(pos, 3) == "cos") {
            pos += 3;
            if (expr[pos] == '(') {
                pos++;
                double arg = parseExpression(expr, pos);
                if (expr[pos] == ')') pos++;
                return cos(arg);
            }
        }
        else if (pos + 3 <= expr.length() && expr.substr(pos, 3) == "tan") {
            pos += 3;
            if (expr[pos] == '(') {
                pos++;
                double arg = parseExpression(expr, pos);
                if (expr[pos] == ')') pos++;
                return tan(arg);
            }
        }
        else if (pos + 3 <= expr.length() && expr.substr(pos, 3) == "log") {
            pos += 3;
            if (expr[pos] == '(') {
                pos++;
                double arg = parseExpression(expr, pos);
                if (expr[pos] == ')') pos++;
                return log10(arg);
            }
        }
        else if (pos + 2 <= expr.length() && expr.substr(pos, 2) == "ln") {
            pos += 2;
            if (expr[pos] == '(') {
                pos++;
                double arg = parseExpression(expr, pos);
                if (expr[pos] == ')') pos++;
                return log(arg);
            }
        }
        
        if (expr[pos] == '(') {
            pos++;
            double result = parseExpression(expr, pos);
            if (expr[pos] == ')') pos++;
            return result;
        }
        
        std::string numStr;
        while (pos < expr.length() && (isdigit(expr[pos]) || expr[pos] == '.')) {
            numStr += expr[pos];
            pos++;
        }
        
        if (!numStr.empty()) {
            return std::stod(numStr);
        }
        
        return 0;
    }
    
    void ProcessCalculations() {
        for (int i = 0; i < lines.size(); i++) {
            std::string& line = lines[i].content;
            size_t startPos = 0;
            std::string newLine = "";
            size_t lastPos = 0;
            
            while (true) {
                size_t openBracket = line.find('[', startPos);
                if (openBracket == std::string::npos) {
                    newLine += line.substr(lastPos);
                    break;
                }
                
                size_t closeBracket = line.find(']', openBracket + 1);
                if (closeBracket == std::string::npos) {
                    newLine += line.substr(lastPos);
                    break;
                }
                
                newLine += line.substr(lastPos, openBracket - lastPos);
                std::string expression = line.substr(openBracket + 1, closeBracket - openBracket - 1);
                
                try {
                    double result = evaluateExpression(expression);
                    std::string resultStr;
                    if (std::abs(result - std::round(result)) < 0.0001) {
                        resultStr = std::to_string((int)std::round(result));
                    } else {
                        resultStr = std::to_string(result);
                        resultStr.erase(resultStr.find_last_not_of('0') + 1, std::string::npos);
                        if (resultStr.back() == '.') resultStr.pop_back();
                    }
                    newLine += "[" + expression + "=" + resultStr + "]";
                } catch (...) {
                    newLine += "[" + expression + "=ERROR]";
                }
                
                startPos = closeBracket + 1;
                lastPos = startPos;
            }
            line = newLine;
        }
        std::cout << "Calculations processed!" << std::endl;
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
            DrawingPoint newPoint = {pos, thickness, color, lineIndex, offsetY};
            lines[lineIndex].drawings.push_back(newPoint);
        }
    }
    
    void EraseAtPosition(Vector2 pos) {
        float eraserSize = ERASER_SIZE;
        for (auto& line : lines) {
            line.drawings.erase(std::remove_if(line.drawings.begin(), line.drawings.end(),
                [pos, eraserSize](const DrawingPoint& point) {
                    float dist = sqrt(pow(pos.x - point.position.x, 2) + pow(pos.y - point.position.y, 2));
                    return dist < eraserSize;
                }), line.drawings.end());
        }
    }
    
    void SaveFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            bool themeMode = darkMode;
            file.write((char*)&themeMode, sizeof(bool));
            int lineCount = lines.size();
            file.write((char*)&lineCount, sizeof(int));
            for (const auto& line : lines) {
                int textLength = line.content.length();
                file.write((char*)&textLength, sizeof(int));
                file.write(line.content.c_str(), textLength);
                int drawingCount = line.drawings.size();
                file.write((char*)&drawingCount, sizeof(int));
                for (const auto& drawing : line.drawings) {
                    file.write((char*)&drawing.position, sizeof(Vector2));
                    file.write((char*)&drawing.thickness, sizeof(float));
                    file.write((char*)&drawing.color, sizeof(Color));
                    file.write((char*)&drawing.relativeLine, sizeof(int));
                    file.write((char*)&drawing.offsetY, sizeof(float));
                }
            }
            file.close();
            std::cout << "File saved: " << filename << std::endl;
        }
    }
    
    void LoadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (file.is_open()) {
            lines.clear();
            bool savedTheme;
            file.read((char*)&savedTheme, sizeof(bool));
            darkMode = savedTheme;
            int lineCount;
            file.read((char*)&lineCount, sizeof(int));
            for (int i = 0; i < lineCount; i++) {
                TextLine line;
                int textLength;
                file.read((char*)&textLength, sizeof(int));
                line.content.resize(textLength);
                file.read(&line.content[0], textLength);
                int drawingCount;
                file.read((char*)&drawingCount, sizeof(int));
                for (int j = 0; j < drawingCount; j++) {
                    DrawingPoint drawing;
                    file.read((char*)&drawing.position, sizeof(Vector2));
                    file.read((char*)&drawing.thickness, sizeof(float));
                    file.read((char*)&drawing.color, sizeof(Color));
                    file.read((char*)&drawing.relativeLine, sizeof(int));
                    file.read((char*)&drawing.offsetY, sizeof(float));
                    line.drawings.push_back(drawing);
                }
                lines.push_back(line);
            }
            file.close();
            std::cout << "File loaded: " << filename << std::endl;
            cursorLine = cursorPos = 0;
            scrollOffset = 0;
            hasSelection = false;
        }
    }
    
    void HandleBackspace() {
        if (hasSelection) {
            DeleteSelection();
        } else if (cursorPos > 0) {
            // UTF-8 sicher löschen
            std::string& line = lines[cursorLine].content;
            int bytePos = 0;
            int charCount = 0;
            while (bytePos < line.length() && charCount < cursorPos - 1) {
                if ((line[bytePos] & 0xF8) == 0xF0) bytePos += 4;
                else if ((line[bytePos] & 0xF0) == 0xE0) bytePos += 3;
                else if ((line[bytePos] & 0xE0) == 0xC0) bytePos += 2;
                else bytePos += 1;
                charCount++;
            }
            
            int nextBytePos = bytePos;
            if (nextBytePos < line.length()) {
                if ((line[nextBytePos] & 0xF8) == 0xF0) nextBytePos += 4;
                else if ((line[nextBytePos] & 0xF0) == 0xE0) nextBytePos += 3;
                else if ((line[nextBytePos] & 0xE0) == 0xC0) nextBytePos += 2;
                else nextBytePos += 1;
            }
            
            line.erase(bytePos, nextBytePos - bytePos);
            cursorPos--;
        } else if (cursorLine > 0) {
            cursorPos = lines[cursorLine - 1].content.length();
            lines[cursorLine - 1].content += lines[cursorLine].content;
            for (auto& drawing : lines[cursorLine].drawings) {
                lines[cursorLine - 1].drawings.push_back(drawing);
            }
            lines.erase(lines.begin() + cursorLine);
            cursorLine--;
        }
        cursorTimer = 0;
        cursorVisible = true;
    }
    
    void HandleDelete() {
        if (hasSelection) {
            DeleteSelection();
        } else if (cursorPos < GetCharCount(lines[cursorLine].content)) {
            // UTF-8 sicher löschen
            std::string& line = lines[cursorLine].content;
            int bytePos = 0;
            int charCount = 0;
            while (bytePos < line.length() && charCount < cursorPos) {
                if ((line[bytePos] & 0xF8) == 0xF0) bytePos += 4;
                else if ((line[bytePos] & 0xF0) == 0xE0) bytePos += 3;
                else if ((line[bytePos] & 0xE0) == 0xC0) bytePos += 2;
                else bytePos += 1;
                charCount++;
            }
            
            int nextBytePos = bytePos;
            if (nextBytePos < line.length()) {
                if ((line[nextBytePos] & 0xF8) == 0xF0) nextBytePos += 4;
                else if ((line[nextBytePos] & 0xF0) == 0xE0) nextBytePos += 3;
                else if ((line[nextBytePos] & 0xE0) == 0xC0) nextBytePos += 2;
                else nextBytePos += 1;
            }
            
            line.erase(bytePos, nextBytePos - bytePos);
        } else if (cursorLine + 1 < lines.size()) {
            lines[cursorLine].content += lines[cursorLine + 1].content;
            for (auto& drawing : lines[cursorLine + 1].drawings) {
                lines[cursorLine].drawings.push_back(drawing);
            }
            lines.erase(lines.begin() + cursorLine + 1);
        }
        cursorTimer = 0;
        cursorVisible = true;
    }
    
    int GetCharCount(const std::string& text) {
        int count = 0;
        for (size_t i = 0; i < text.length(); ) {
            if ((text[i] & 0xF8) == 0xF0) i += 4;
            else if ((text[i] & 0xF0) == 0xE0) i += 3;
            else if ((text[i] & 0xE0) == 0xC0) i += 2;
            else i += 1;
            count++;
        }
        return count;
    }
    
    void HandleLeftArrow() {
        if (cursorPos > 0) {
            cursorPos--;
        } else if (cursorLine > 0) {
            cursorLine--;
            cursorPos = GetCharCount(lines[cursorLine].content);
        }
        cursorTimer = 0;
        cursorVisible = true;
    }
    
    void HandleRightArrow() {
        if (cursorPos < GetCharCount(lines[cursorLine].content)) {
            cursorPos++;
        } else if (cursorLine + 1 < lines.size()) {
            cursorLine++;
            cursorPos = 0;
        }
        cursorTimer = 0;
        cursorVisible = true;
    }
    
    void DrawContent() {
        // Zeichnungen
        for (int i = 0; i < lines.size(); i++) {
            int lineY = textAreaStartY + (i - scrollOffset) * lineHeight;
            if (lineY + lineHeight < textAreaStartY || lineY > screenHeight) continue;
            for (const auto& drawing : lines[i].drawings) {
                float yPos = lineY + drawing.offsetY;
                DrawCircleV({drawing.position.x, yPos}, drawing.thickness / 2, drawing.color);
            }
        }
        
        // Text
        for (int i = 0; i < visibleLines && (scrollOffset + i) < lines.size(); i++) {
            int lineNumber = scrollOffset + i;
            int yPos = textAreaStartY + i * lineHeight;
            
            // Zeilennummer
            DrawText(TextFormat("%d", lineNumber + 1), 
                    10, yPos, fontSize - 8, LIGHTGRAY);
            
            const std::string& text = lines[lineNumber].content;
            float currentX = textAreaStartX;
            int charIndex = 0;
            size_t bytePos = 0;
            
            while (bytePos < text.length()) {
                // UTF-8 Zeichenlänge
                int charLen = 1;
                if ((text[bytePos] & 0xF8) == 0xF0) charLen = 4;
                else if ((text[bytePos] & 0xF0) == 0xE0) charLen = 3;
                else if ((text[bytePos] & 0xE0) == 0xC0) charLen = 2;
                
                std::string utf8Char = text.substr(bytePos, charLen);
                float charWidth = GetCharWidth(utf8Char);
                
                // Prüfe Selektion
                bool isSelected = hasSelection && 
                    lineNumber >= selectionStartLine && lineNumber <= selectionEndLine &&
                    charIndex >= (lineNumber == selectionStartLine ? selectionStartPos : 0) &&
                    charIndex < (lineNumber == selectionEndLine ? selectionEndPos : GetCharCount(text));
                
                if (isSelected) {
                    DrawRectangle(currentX, yPos - 2, charWidth, lineHeight, ColorAlpha(BLUE, 0.5f));
                    DrawTextEx(font, utf8Char.c_str(), {currentX, float(yPos)}, fontSize, charSpacing, WHITE);
                } else {
                    DrawTextEx(font, utf8Char.c_str(), {currentX, float(yPos)}, fontSize, charSpacing, BLACK);
                }
                currentX += charWidth;
                bytePos += charLen;
                charIndex++;
            }
            
            // Cursor
            if (cursorLine == lineNumber && cursorVisible && !hasSelection) {
                int cursorX = GetCursorX(lineNumber, cursorPos);
                DrawRectangle(cursorX, yPos, 2, fontSize, BLACK);
            }
        }
        
        // Radierer Cursor
        if (isErasing) {
            Vector2 mousePos = GetMousePosition();
            DrawCircleV(mousePos, ERASER_SIZE, ColorAlpha(RED, 0.3f));
            DrawCircleLines(mousePos.x, mousePos.y, ERASER_SIZE, RED);
            DrawText("ERASING", mousePos.x - 30, mousePos.y - 25, 15, RED);
        }
        
        // Pinselvorschau
        if (isDrawing) {
            Vector2 mousePos = GetMousePosition();
            DrawCircleV(mousePos, currentThickness / 2, ColorAlpha(BLACK, 0.5f));
            DrawText(TextFormat("Brush Size: %.0f", currentThickness), 10, screenHeight - 85, 15, BLACK);
        }
        
        // Scrollbar
        Rectangle scrollBarRect = {
            (float)(screenWidth - scrollBarWidth - 10),
            (float)(textAreaStartY),
            (float)scrollBarWidth,
            (float)(visibleLines * lineHeight)
        };
        DrawRectangleRec(scrollBarRect, LIGHTGRAY);
        
        int maxScroll = std::max(0, (int)lines.size() - visibleLines);
        if (maxScroll > 0) {
            float scrollBarHeight = scrollBarRect.height * ((float)visibleLines / lines.size());
            float scrollBarY = scrollBarRect.y + scrollPos * (scrollBarRect.height - scrollBarHeight);
            DrawRectangle(screenWidth - scrollBarWidth - 10, scrollBarY, scrollBarWidth, scrollBarHeight, DARKGRAY);
        }
        
        // Resize Grip
        DrawTriangle(
            {float(screenWidth - 15), float(screenHeight - 5)},
            {float(screenWidth - 5), float(screenHeight - 15)},
            {float(screenWidth - 5), float(screenHeight - 5)},
            GRAY
        );
        
        // Hilfe (jetzt mit Sonderzeichen-Hinweis)
        DrawText("Ctrl+S: Save | Ctrl+O: Load | Ctrl+T: Theme | Ctrl+R: Calculate | F11: Fullscreen", 10, screenHeight - 85, 15, BLACK);
        DrawText("Ctrl+C: Copy | Ctrl+X: Cut | Ctrl+V: Paste | Ctrl+D: Clear Drawings", 10, screenHeight - 70, 15, BLACK);
        DrawText("Right Click: Draw | Middle Click: Erase | Mouse Wheel: Brush Size | Resize: Drag corner", 10, screenHeight - 55, 15, BLACK);
        DrawText("German Umlauts: ä ö ü ß | AltGr+2=² AltGr+3=³ AltGr+E=€ | Math: [2+2] becomes [2+2=4]", 10, screenHeight - 25, 15, BLACK);

    }
    
    void Update() {
        if (fileDialogActive) {
            return;
        }
        
        UpdateWindow();
        
        // Theme umschalten mit Shader (Ctrl+T)
        if (IsKeyPressed(KEY_T) && IsKeyDown(KEY_LEFT_CONTROL)) {
            darkMode = !darkMode;
            std::cout << "Dark Mode: " << (darkMode ? "ON" : "OFF") << " (Shader Invert)" << std::endl;
        }
        
        // Berechnungen Ctrl+R
        if (IsKeyPressed(KEY_R) && IsKeyDown(KEY_LEFT_CONTROL)) {
            ProcessCalculations();
        }
        
        // Copy/Cut/Paste (Ctrl+C, Ctrl+X, Ctrl+V)
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            if (IsKeyPressed(KEY_C)) {
                clipboard = GetSelectedText();
                std::cout << "Copied to clipboard" << std::endl;
            }
            if (IsKeyPressed(KEY_X)) {
                clipboard = GetSelectedText();
                DeleteSelection();
                std::cout << "Cut to clipboard" << std::endl;
            }
            if (IsKeyPressed(KEY_V)) {
                if (hasSelection) DeleteSelection();
                for (char c : clipboard) {
                    if (c == '\n') {
                        std::string remaining = lines[cursorLine].content.substr(cursorPos);
                        lines[cursorLine].content = lines[cursorLine].content.substr(0, cursorPos);
                        TextLine newLine;
                        newLine.content = remaining;
                        lines.insert(lines.begin() + cursorLine + 1, newLine);
                        cursorLine++;
                        cursorPos = 0;
                    } else {
                        lines[cursorLine].content.insert(cursorPos, 1, c);
                        cursorPos++;
                    }
                }
                std::cout << "Pasted from clipboard" << std::endl;
            }
        }
        
        // Speichern/Laden mit Dialog
        if (IsKeyPressed(KEY_S) && IsKeyDown(KEY_LEFT_CONTROL)) {
            ShowFileDialog(true);
        }
        if (IsKeyPressed(KEY_O) && IsKeyDown(KEY_LEFT_CONTROL)) {
            ShowFileDialog(false);
        }
        
        // Alle Zeichnungen löschen Ctrl+D
        if (IsKeyPressed(KEY_D) && IsKeyDown(KEY_LEFT_CONTROL)) {
            for (auto& line : lines) {
                line.drawings.clear();
            }
            std::cout << "All drawings cleared" << std::endl;
        }
        
        UpdateVisibleLines();
        
        // Cursor Blinken
        cursorTimer += GetFrameTime();
        if (cursorTimer >= 0.5) {
            cursorTimer = 0;
            cursorVisible = !cursorVisible;
        }
        
        Vector2 mousePos = GetMousePosition();
        
        // Radieren mit Mittelklick
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            if (!isErasing) {
                isErasing = true;
                EraseAtPosition(mousePos);
            } else {
                EraseAtPosition(mousePos);
            }
        } else {
            isErasing = false;
        }
        
        // Zeichnen mit Rechtsklick
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !isErasing) {
            if (!isDrawing) {
                isDrawing = true;
                AddDrawingPoint(mousePos, currentThickness, BLACK);
                lastDrawPos = mousePos;
            } else {
                AddDrawingPoint(mousePos, currentThickness, BLACK);
                float distance = sqrt(pow(mousePos.x - lastDrawPos.x, 2) + pow(mousePos.y - lastDrawPos.y, 2));
                if (distance > currentThickness / 2) {
                    int steps = (int)(distance / (currentThickness / 2)) + 1;
                    for (int i = 1; i < steps; i++) {
                        float t = (float)i / steps;
                        Vector2 interpolatedPos = {
                            lastDrawPos.x + t * (mousePos.x - lastDrawPos.x),
                            lastDrawPos.y + t * (mousePos.y - lastDrawPos.y)
                        };
                        AddDrawingPoint(interpolatedPos, currentThickness, BLACK);
                    }
                }
                lastDrawPos = mousePos;
            }
        } else {
            isDrawing = false;
        }
        
        // Pinselgröße mit Mausrad
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            int wheel = GetMouseWheelMove();
            if (wheel != 0) {
                currentThickness += wheel * 2;
                currentThickness = std::max(4.0f, std::min(40.0f, currentThickness));
            }
        }
        
        // Sonderzeichen Eingabe
        HandleSpecialChars();
        
        // Normale Zeichen (ASCII)
        int key = GetCharPressed();
        while (key > 0) {
            if (hasSelection) DeleteSelection();
            if (key >= 32 && key <= 125) {
                lines[cursorLine].content.insert(cursorPos, 1, (char)key);
                cursorPos++;
            }
            key = GetCharPressed();
        }
        
        // Tastenwiederholung für Backspace
        if (IsKeyPressed(KEY_BACKSPACE)) {
            HandleBackspace();
            lastKeyPressed = KEY_BACKSPACE;
            lastKeyTime = GetTime();
            keyRepeatActive = true;
        } else if (keyRepeatActive && IsKeyDown(KEY_BACKSPACE) && lastKeyPressed == KEY_BACKSPACE) {
            double currentTime = GetTime();
            if (currentTime - lastKeyTime > keyRepeatDelay) {
                HandleBackspace();
                lastKeyTime = currentTime;
            }
        }
        
        // Tastenwiederholung für Delete
        if (IsKeyPressed(KEY_DELETE)) {
            HandleDelete();
            lastKeyPressed = KEY_DELETE;
            lastKeyTime = GetTime();
            keyRepeatActive = true;
        } else if (keyRepeatActive && IsKeyDown(KEY_DELETE) && lastKeyPressed == KEY_DELETE) {
            double currentTime = GetTime();
            if (currentTime - lastKeyTime > keyRepeatDelay) {
                HandleDelete();
                lastKeyTime = currentTime;
            }
        }
        
        // Tastenwiederholung für Pfeiltasten
        if (IsKeyPressed(KEY_LEFT)) {
            if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) {
                cursorLine = selectionStartLine;
                cursorPos = selectionStartPos;
                hasSelection = false;
            } else {
                HandleLeftArrow();
            }
            lastKeyPressed = KEY_LEFT;
            lastKeyTime = GetTime();
            keyRepeatActive = true;
            
            if (IsKeyDown(KEY_LEFT_SHIFT) && !hasSelection) {
                selectionStartLine = selectionEndLine = cursorLine;
                selectionStartPos = selectionEndPos = cursorPos;
                hasSelection = true;
            }
        } else if (keyRepeatActive && IsKeyDown(KEY_LEFT) && lastKeyPressed == KEY_LEFT) {
            double currentTime = GetTime();
            if (currentTime - lastKeyTime > keyRepeatDelay) {
                if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) {
                    cursorLine = selectionStartLine;
                    cursorPos = selectionStartPos;
                    hasSelection = false;
                } else {
                    HandleLeftArrow();
                }
                lastKeyTime = currentTime;
            }
        }
        
        if (IsKeyPressed(KEY_RIGHT)) {
            if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) {
                cursorLine = selectionEndLine;
                cursorPos = selectionEndPos;
                hasSelection = false;
            } else {
                HandleRightArrow();
            }
            lastKeyPressed = KEY_RIGHT;
            lastKeyTime = GetTime();
            keyRepeatActive = true;
            
            if (IsKeyDown(KEY_LEFT_SHIFT) && !hasSelection) {
                selectionStartLine = selectionEndLine = cursorLine;
                selectionStartPos = selectionEndPos = cursorPos;
                hasSelection = true;
            }
        } else if (keyRepeatActive && IsKeyDown(KEY_RIGHT) && lastKeyPressed == KEY_RIGHT) {
            double currentTime = GetTime();
            if (currentTime - lastKeyTime > keyRepeatDelay) {
                if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) {
                    cursorLine = selectionEndLine;
                    cursorPos = selectionEndPos;
                    hasSelection = false;
                } else {
                    HandleRightArrow();
                }
                lastKeyTime = currentTime;
            }
        }
        
        // Reset key repeat wenn keine Taste mehr gedrückt
        if (!IsKeyDown(KEY_BACKSPACE) && !IsKeyDown(KEY_DELETE) && 
            !IsKeyDown(KEY_LEFT) && !IsKeyDown(KEY_RIGHT)) {
            keyRepeatActive = false;
        }
        
        // Enter
        if (IsKeyPressed(KEY_ENTER)) {
            if (hasSelection) DeleteSelection();
            std::string remaining = lines[cursorLine].content.substr(cursorPos);
            lines[cursorLine].content = lines[cursorLine].content.substr(0, cursorPos);
            TextLine newLine;
            newLine.content = remaining;
            lines.insert(lines.begin() + cursorLine + 1, newLine);
            cursorLine++;
            cursorPos = 0;
            cursorTimer = 0;
            cursorVisible = true;
        }
        
        // Up/Down Pfeile
        if (IsKeyPressed(KEY_UP)) {
            if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) {
                cursorLine = selectionStartLine;
                cursorPos = selectionStartPos;
                hasSelection = false;
            } else if (cursorLine > 0) {
                cursorLine--;
                int lineLen = GetCharCount(lines[cursorLine].content);
                cursorPos = std::min(cursorPos, lineLen);
            }
            cursorTimer = 0;
            cursorVisible = true;
        }
        
        if (IsKeyPressed(KEY_DOWN)) {
            if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) {
                cursorLine = selectionEndLine;
                cursorPos = selectionEndPos;
                hasSelection = false;
            } else if (cursorLine + 1 < lines.size()) {
                cursorLine++;
                int lineLen = GetCharCount(lines[cursorLine].content);
                cursorPos = std::min(cursorPos, lineLen);
            }
            cursorTimer = 0;
            cursorVisible = true;
        }
        
        // Textauswahl mit Maus (UTF-8 safe)
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (mousePos.x >= textAreaStartX && mousePos.y >= textAreaStartY) {
                int relativeY = (int)(mousePos.y - textAreaStartY);
                int line = relativeY / lineHeight + scrollOffset;
                if (line >= 0 && line < lines.size()) {
                    int pos = FindPosAtX(line, mousePos.x);
                    selectionStartLine = selectionEndLine = line;
                    selectionStartPos = selectionEndPos = pos;
                    hasSelection = true;
                    cursorLine = line;
                    cursorPos = pos;
                }
            }
        }
        
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hasSelection) {
            if (mousePos.x >= textAreaStartX && mousePos.y >= textAreaStartY) {
                int relativeY = (int)(mousePos.y - textAreaStartY);
                int line = relativeY / lineHeight + scrollOffset;
                if (line >= 0 && line < lines.size()) {
                    int pos = FindPosAtX(line, mousePos.x);
                    selectionEndLine = line;
                    selectionEndPos = pos;
                    cursorLine = line;
                    cursorPos = pos;
                    if (selectionStartLine > selectionEndLine ||
                        (selectionStartLine == selectionEndLine && selectionStartPos > selectionEndPos)) {
                        std::swap(selectionStartLine, selectionEndLine);
                        std::swap(selectionStartPos, selectionEndPos);
                    }
                }
            }
        }
        
        // Scrolling
        int mouseWheel = GetMouseWheelMove();
        if (mouseWheel != 0 && !isDrawing && !isErasing) {
            scrollOffset -= mouseWheel * 2;
            int maxScroll = std::max(0, (int)lines.size() - visibleLines);
            scrollOffset = std::max(0, std::min(maxScroll, scrollOffset));
            if (maxScroll > 0) {
                scrollPos = (float)scrollOffset / maxScroll;
            }
        }
    }
    
    std::string GetSelectedText() {
        if (!hasSelection) return "";
        
        std::string selected;
        for (int l = selectionStartLine; l <= selectionEndLine; l++) {
            if (l < lines.size()) {
                const std::string& line = lines[l].content;
                int start = (l == selectionStartLine) ? selectionStartPos : 0;
                int end = (l == selectionEndLine) ? selectionEndPos : GetCharCount(line);
                
                // UTF-8 Positionen finden
                int byteStart = 0;
                int charCount = 0;
                while (byteStart < line.length() && charCount < start) {
                    if ((line[byteStart] & 0xF8) == 0xF0) byteStart += 4;
                    else if ((line[byteStart] & 0xF0) == 0xE0) byteStart += 3;
                    else if ((line[byteStart] & 0xE0) == 0xC0) byteStart += 2;
                    else byteStart += 1;
                    charCount++;
                }
                
                int byteEnd = byteStart;
                while (byteEnd < line.length() && charCount < end) {
                    if ((line[byteEnd] & 0xF8) == 0xF0) byteEnd += 4;
                    else if ((line[byteEnd] & 0xF0) == 0xE0) byteEnd += 3;
                    else if ((line[byteEnd] & 0xE0) == 0xC0) byteEnd += 2;
                    else byteEnd += 1;
                    charCount++;
                }
                
                selected += line.substr(byteStart, byteEnd - byteStart);
                if (l < selectionEndLine) selected += "\n";
            }
        }
        return selected;
    }
    
    void DeleteSelection() {
        if (!hasSelection) return;
        
        std::string beforeSelection;
        std::string afterSelection;
        
        // UTF-8 sicher extrahieren
        const std::string& startLine = lines[selectionStartLine].content;
        int byteStart = 0;
        int charCount = 0;
        while (byteStart < startLine.length() && charCount < selectionStartPos) {
            if ((startLine[byteStart] & 0xF8) == 0xF0) byteStart += 4;
            else if ((startLine[byteStart] & 0xF0) == 0xE0) byteStart += 3;
            else if ((startLine[byteStart] & 0xE0) == 0xC0) byteStart += 2;
            else byteStart += 1;
            charCount++;
        }
        
        const std::string& endLine = lines[selectionEndLine].content;
        int byteEnd = 0;
        charCount = 0;
        while (byteEnd < endLine.length() && charCount < selectionEndPos) {
            if ((endLine[byteEnd] & 0xF8) == 0xF0) byteEnd += 4;
            else if ((endLine[byteEnd] & 0xF0) == 0xE0) byteEnd += 3;
            else if ((endLine[byteEnd] & 0xE0) == 0xC0) byteEnd += 2;
            else byteEnd += 1;
            charCount++;
        }
        
        beforeSelection = startLine.substr(0, byteStart);
        afterSelection = endLine.substr(byteEnd);
        
        lines[selectionStartLine].content = beforeSelection + afterSelection;
        
        for (int l = selectionEndLine; l > selectionStartLine; l--) {
            for (auto& drawing : lines[l].drawings) {
                lines[selectionStartLine].drawings.push_back(drawing);
            }
            lines.erase(lines.begin() + l);
        }
        
        cursorLine = selectionStartLine;
        cursorPos = selectionStartPos;
        hasSelection = false;
    }
    
    void Draw() {
        if (darkMode) {
            // Dark Mode: Render zuerst auf Textur, dann mit Shader invertieren
            BeginTextureMode(target);
            ClearBackground(RAYWHITE);
            DrawContent();
            EndTextureMode();
            
            BeginDrawing();
            ClearBackground(BLACK);
            BeginShaderMode(invertShader);
            DrawTextureRec(target.texture, Rectangle{0, 0, (float)screenWidth, (float)-screenHeight}, Vector2{0, 0}, WHITE);
            EndShaderMode();
            
            // Dark Mode Text für Dialog (muss im Dark Mode normal gerendert werden)
            if (showFileDialog) {
                HandleFileDialog();
            }
            
            EndDrawing();
        } else {
            // Light Mode: Normal rendern
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawContent();
            
            // Datei-Dialog im Light Mode
            if (showFileDialog) {
                HandleFileDialog();
            }
            
            EndDrawing();
        }
        
        // Datei-Dialog Flag zurücksetzen
        if (showFileDialog && !fileDialogActive) {
            showFileDialog = false;
        }
    }
};

int main() {
    const int screenWidth = 1200;
    const int screenHeight = 800;
    
    InitWindow(screenWidth, screenHeight, "Complete Text Editor with Special Chars");
    SetTargetFPS(60);
    
    TextEditor editor(screenWidth, screenHeight);
    
    while (!WindowShouldClose()) {
        editor.Update();
        editor.Draw();
    }
    
    CloseWindow();
    return 0;
}