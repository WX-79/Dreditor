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
        
        fontSize = 28;
        lineHeight = fontSize + 12;
        charSpacing = 4;
        textAreaStartX = 50;
        textAreaStartY = 30;
        scrollBarWidth = 15;
        
        // Tasten-Wiederholung
        lastKeyTime = 0;
        keyRepeatDelay = 0.15;
        keyRepeatInterval = 0.015;
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
        
        // System-Font laden
        font = GetFontDefault();
        if (FONT_PATH) {
            Font loadedFont = LoadFontEx(FONT_PATH, fontSize, 0, 250);
            if (loadedFont.texture.id != 0) {
                font = loadedFont;
                std::cout << "Loaded system font" << std::endl;
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
        }
        
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            isResizing = false;
        }
        
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
    
    // Korrekte Messung eines einzelnen Zeichens MIT Zeichenabstand
    float GetCharWidth(char c) {
        return MeasureTextEx(font, std::string(1, c).c_str(), fontSize, charSpacing).x;
    }
    
    // Korrekte Messung eines gesamten Textes MIT Zeichenabstand
    float MeasureTextWidth(const std::string& text) {
        float width = 0;
        for (char c : text) {
            width += GetCharWidth(c);
        }
        return width;
    }
    
    // Exakte Cursor-Position - Jedes Zeichen einzeln berechnen
    int GetCursorX(int line, int pos) {
        if (line >= lines.size()) return textAreaStartX;
        if (pos < 0) pos = 0;
        if (pos > (int)lines[line].content.length()) pos = lines[line].content.length();
        
        const std::string& text = lines[line].content;
        float width = 0;
        // Jedes Zeichen bis zur Cursor-Position einzeln messen
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
    
    // Rechner Implementierung
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
        std::cout << "Calculations done!" << std::endl;
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
            std::cout << "Saved: " << filename << std::endl;
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
            std::cout << "Loaded: " << filename << std::endl;
            cursorLine = cursorPos = 0; scrollOffset = 0; hasSelection = false;
        }
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
        cursorTimer = 0; cursorVisible = true;
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
        cursorTimer = 0; cursorVisible = true;
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
            
            // Jedes Zeichen einzeln zeichnen
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
            
            // Cursor - EXAKTE Position
            if (cursorLine == line && cursorVisible && !hasSelection) {
                int cursorX = GetCursorX(line, cursorPos);
                DrawRectangle(cursorX, y, 2, fontSize, BLACK);
            }
        }
        
        // UI Elemente
        if (isErasing) {
            Vector2 mp = GetMousePosition();
            DrawCircleV(mp, ERASER_SIZE, ColorAlpha(RED, 0.3f));
            DrawCircleLines(mp.x, mp.y, ERASER_SIZE, RED);
            DrawText("ERASING", mp.x - 30, mp.y - 25, 15, RED);
        }
        if (isDrawing) {
            Vector2 mp = GetMousePosition();
            DrawCircleV(mp, currentThickness / 2, ColorAlpha(BLACK, 0.5f));
            DrawText(TextFormat("Brush: %.0f", currentThickness), 10, screenHeight - 85, 15, BLACK);
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
        
        // Hilfe
        DrawText("Ctrl+S: Save | Ctrl+O: Load | Ctrl+T: Theme | Ctrl+R: Calculate | F11: Fullscreen", 10, screenHeight - 70, 15, BLACK);
        DrawText("Ctrl+C: Copy | Ctrl+X: Cut | Ctrl+V: Paste | Ctrl+D: Clear Drawings", 10, screenHeight - 55, 15, BLACK);
        DrawText("Right Click: Draw | Middle Click: Erase | Mouse Wheel: Brush Size", 10, screenHeight - 40, 15, BLACK);
        DrawText("Hold Backspace/Delete for repeat | Arrow keys + Shift for selection", 10, screenHeight - 25, 15, BLACK);
        DrawText("Math: [2+2] becomes [2+2=4] | sqrt(9) | exp(2,3) = 2^3", 10, screenHeight - 10, 15, BLACK);
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
    }
    
    void Update() {
        if (fileDialogActive) return;
        
        UpdateWindow();
        
        // Shortcuts
        if (IsKeyPressed(KEY_T) && IsKeyDown(KEY_LEFT_CONTROL)) { 
            darkMode = !darkMode; 
            std::cout << (darkMode ? "Dark Mode ON" : "Light Mode ON") << std::endl; 
        }
        if (IsKeyPressed(KEY_R) && IsKeyDown(KEY_LEFT_CONTROL)) ProcessCalculations();
        if (IsKeyPressed(KEY_S) && IsKeyDown(KEY_LEFT_CONTROL)) ShowFileDialog(true);
        if (IsKeyPressed(KEY_O) && IsKeyDown(KEY_LEFT_CONTROL)) ShowFileDialog(false);
        if (IsKeyPressed(KEY_D) && IsKeyDown(KEY_LEFT_CONTROL)) { 
            for (auto& l : lines) l.drawings.clear(); 
            std::cout << "Drawings cleared" << std::endl; 
        }
        
        // Copy/Paste
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            if (IsKeyPressed(KEY_C)) { 
                clipboard = GetSelectedText(); 
                std::cout << "Copied" << std::endl; 
            }
            if (IsKeyPressed(KEY_X)) { 
                clipboard = GetSelectedText(); 
                DeleteSelection(); 
                std::cout << "Cut" << std::endl; 
            }
            if (IsKeyPressed(KEY_V)) {
                if (hasSelection) DeleteSelection();
                for (char c : clipboard) {
                    if (c == '\n') {
                        std::string rest = lines[cursorLine].content.substr(cursorPos);
                        lines[cursorLine].content = lines[cursorLine].content.substr(0, cursorPos);
                        lines.insert(lines.begin() + cursorLine + 1, {rest});
                        cursorLine++; cursorPos = 0;
                    } else { 
                        lines[cursorLine].content.insert(cursorPos, 1, c); 
                        cursorPos++; 
                    }
                }
                std::cout << "Pasted" << std::endl;
            }
        }
        
        UpdateVisibleLines();
        cursorTimer += GetFrameTime();
        if (cursorTimer >= 0.5) { cursorTimer = 0; cursorVisible = !cursorVisible; }
        
        Vector2 mp = GetMousePosition();
        
        // Radieren
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) { 
            if (!isErasing) { isErasing = true; EraseAtPosition(mp); } 
            else EraseAtPosition(mp); 
        } else isErasing = false;
        
        // Zeichnen
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !isErasing) {
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
        } else isDrawing = false;
        
        // Pinselgröße
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) { 
            int w = GetMouseWheelMove(); 
            if (w != 0) { 
                currentThickness += w * 2; 
                currentThickness = std::max(4.0f, std::min(40.0f, currentThickness)); 
            } 
        }
        
        // Normale Zeicheneingabe
        int key = GetCharPressed();
        while (key > 0) {
            if (hasSelection) DeleteSelection();
            if (key >= 32 && key <= 125) {
                lines[cursorLine].content.insert(cursorPos, 1, (char)key);
                cursorPos++;
                cursorTimer = 0; cursorVisible = true;
            }
            key = GetCharPressed();
        }
        
        // Tastenwiederholung Backspace
        if (IsKeyPressed(KEY_BACKSPACE)) { 
            HandleBackspace(); 
            lastKeyPressed = KEY_BACKSPACE; 
            lastKeyTime = GetTime(); 
            keyRepeatActive = true; 
        } else if (keyRepeatActive && IsKeyDown(KEY_BACKSPACE) && lastKeyPressed == KEY_BACKSPACE) {
            double now = GetTime();
            if (now - lastKeyTime > keyRepeatDelay) { 
                HandleBackspace(); 
                lastKeyTime = now; 
            }
        }
        
        // Tastenwiederholung Delete
        if (IsKeyPressed(KEY_DELETE)) { 
            HandleDelete(); 
            lastKeyPressed = KEY_DELETE; 
            lastKeyTime = GetTime(); 
            keyRepeatActive = true; 
        } else if (keyRepeatActive && IsKeyDown(KEY_DELETE) && lastKeyPressed == KEY_DELETE) {
            double now = GetTime();
            if (now - lastKeyTime > keyRepeatDelay) { 
                HandleDelete(); 
                lastKeyTime = now; 
            }
        }
        
        // Pfeiltasten
        if (IsKeyPressed(KEY_LEFT)) {
            if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) { 
                cursorLine = selectionStartLine; 
                cursorPos = selectionStartPos; 
                hasSelection = false; 
            } else { 
                if (cursorPos > 0) cursorPos--; 
                else if (cursorLine > 0) { 
                    cursorLine--; 
                    cursorPos = lines[cursorLine].content.length(); 
                } 
            }
            if (IsKeyDown(KEY_LEFT_SHIFT) && !hasSelection) { 
                selectionStartLine = selectionEndLine = cursorLine; 
                selectionStartPos = selectionEndPos = cursorPos; 
                hasSelection = true; 
            }
            lastKeyPressed = KEY_LEFT; 
            lastKeyTime = GetTime(); 
            keyRepeatActive = true;
            cursorTimer = 0; cursorVisible = true;
        } else if (keyRepeatActive && IsKeyDown(KEY_LEFT) && lastKeyPressed == KEY_LEFT) {
            double now = GetTime();
            if (now - lastKeyTime > keyRepeatDelay) {
                if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) { 
                    cursorLine = selectionStartLine; 
                    cursorPos = selectionStartPos; 
                    hasSelection = false; 
                } else { 
                    if (cursorPos > 0) cursorPos--; 
                    else if (cursorLine > 0) { 
                        cursorLine--; 
                        cursorPos = lines[cursorLine].content.length(); 
                    } 
                }
                lastKeyTime = now;
                cursorTimer = 0; cursorVisible = true;
            }
        }
        
        if (IsKeyPressed(KEY_RIGHT)) {
            if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) { 
                cursorLine = selectionEndLine; 
                cursorPos = selectionEndPos; 
                hasSelection = false; 
            } else { 
                if (cursorPos < (int)lines[cursorLine].content.length()) cursorPos++; 
                else if (cursorLine + 1 < lines.size()) { 
                    cursorLine++; 
                    cursorPos = 0; 
                } 
            }
            if (IsKeyDown(KEY_LEFT_SHIFT) && !hasSelection) { 
                selectionStartLine = selectionEndLine = cursorLine; 
                selectionStartPos = selectionEndPos = cursorPos; 
                hasSelection = true; 
            }
            lastKeyPressed = KEY_RIGHT; 
            lastKeyTime = GetTime(); 
            keyRepeatActive = true;
            cursorTimer = 0; cursorVisible = true;
        } else if (keyRepeatActive && IsKeyDown(KEY_RIGHT) && lastKeyPressed == KEY_RIGHT) {
            double now = GetTime();
            if (now - lastKeyTime > keyRepeatDelay) {
                if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) { 
                    cursorLine = selectionEndLine; 
                    cursorPos = selectionEndPos; 
                    hasSelection = false; 
                } else { 
                    if (cursorPos < (int)lines[cursorLine].content.length()) cursorPos++; 
                    else if (cursorLine + 1 < lines.size()) { 
                        cursorLine++; 
                        cursorPos = 0; 
                    } 
                }
                lastKeyTime = now;
                cursorTimer = 0; cursorVisible = true;
            }
        }
        
        // Reset key repeat
        if (!IsKeyDown(KEY_BACKSPACE) && !IsKeyDown(KEY_DELETE) && !IsKeyDown(KEY_LEFT) && !IsKeyDown(KEY_RIGHT)) {
            keyRepeatActive = false;
        }
        
        // Enter
        if (IsKeyPressed(KEY_ENTER)) {
            if (hasSelection) DeleteSelection();
            std::string rest = lines[cursorLine].content.substr(cursorPos);
            lines[cursorLine].content = lines[cursorLine].content.substr(0, cursorPos);
            lines.insert(lines.begin() + cursorLine + 1, {rest});
            cursorLine++; cursorPos = 0;
            cursorTimer = 0; cursorVisible = true;
        }
        
        // Up/Down
        if (IsKeyPressed(KEY_UP)) {
            if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) { 
                cursorLine = selectionStartLine; 
                cursorPos = selectionStartPos; 
                hasSelection = false; 
            } else if (cursorLine > 0) { 
                cursorLine--; 
                cursorPos = std::min(cursorPos, (int)lines[cursorLine].content.length()); 
            }
            cursorTimer = 0; cursorVisible = true;
        }
        
        if (IsKeyPressed(KEY_DOWN)) {
            if (hasSelection && !IsKeyDown(KEY_LEFT_SHIFT)) { 
                cursorLine = selectionEndLine; 
                cursorPos = selectionEndPos; 
                hasSelection = false; 
            } else if (cursorLine + 1 < lines.size()) { 
                cursorLine++; 
                cursorPos = std::min(cursorPos, (int)lines[cursorLine].content.length()); 
            }
            cursorTimer = 0; cursorVisible = true;
        }
        
        // Mausauswahl
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mp.x >= textAreaStartX && mp.y >= textAreaStartY) {
            int line = (int)(mp.y - textAreaStartY) / lineHeight + scrollOffset;
            if (line >= 0 && line < lines.size()) {
                int pos = FindPosAtX(line, mp.x);
                selectionStartLine = selectionEndLine = line;
                selectionStartPos = selectionEndPos = pos;
                hasSelection = true;
                cursorLine = line; cursorPos = pos;
            }
        }
        
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hasSelection && mp.x >= textAreaStartX && mp.y >= textAreaStartY) {
            int line = (int)(mp.y - textAreaStartY) / lineHeight + scrollOffset;
            if (line >= 0 && line < lines.size()) {
                int pos = FindPosAtX(line, mp.x);
                selectionEndLine = line; selectionEndPos = pos;
                cursorLine = line; cursorPos = pos;
                if (selectionStartLine > selectionEndLine || (selectionStartLine == selectionEndLine && selectionStartPos > selectionEndPos)) {
                    std::swap(selectionStartLine, selectionEndLine);
                    std::swap(selectionStartPos, selectionEndPos);
                }
            }
        }
        
        // Scrollen
        int wheel = GetMouseWheelMove();
        if (wheel != 0 && !isDrawing && !isErasing) {
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
    InitWindow(1200, 800, "Text Editor - Fixed Cursor");
    SetTargetFPS(60);
    TextEditor editor(1200, 800);
    while (!WindowShouldClose()) { editor.Update(); editor.Draw(); }
    CloseWindow();
    return 0;
}
