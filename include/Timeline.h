#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "Canvas.h"

namespace Anima {

    struct FrameSnapshot {
        std::vector<Pixel> pixels;
    };

    struct UndoRecord {
        int frameIndex;
        std::vector<Pixel> before;
    };

    class Timeline {
    public:
        int  currentFrame = 0;
        int  fps = 12;
        bool playing = false;
        bool looping = true;
        float playTimer = 0.0f;

        bool onionBack = true;
        bool onionForward = false;
        float onionAlpha = 0.28f;

        std::vector<std::unique_ptr<Canvas>> frames;

        static constexpr int MAX_UNDO = 40;
        std::vector<UndoRecord> undoStack;
        std::vector<UndoRecord> redoStack;

        void addFrame(int w, int h) {
            auto f = std::make_unique<Canvas>();
            f->Init(w, h);
            f->Clear();
            frames.push_back(std::move(f));
        }

        void duplicateFrame(int index) {
            if (index < 0 || index >= (int)frames.size()) return;
            auto f = std::make_unique<Canvas>();
            f->Init(frames[index]->Width(), frames[index]->Height());
            f->Pixels() = frames[index]->Pixels();
            f->UploadTexture();
            frames.insert(frames.begin() + index + 1, std::move(f));
        }

        void deleteFrame(int index) {
            if (frames.size() <= 1) return;
            frames.erase(frames.begin() + index);
            currentFrame = std::clamp(currentFrame, 0, (int)frames.size() - 1);
        }

        Canvas* current() {
            if (frames.empty() || currentFrame < 0 || currentFrame >= (int)frames.size())
                return nullptr;
            return frames[currentFrame].get();
        }

        Canvas* at(int i) {
            if (i < 0 || i >= (int)frames.size()) return nullptr;
            return frames[i].get();
        }

        int frameCount() const { return (int)frames.size(); }

        void advance(float dt) {
            if (!playing || frames.empty()) return;
            playTimer += dt;
            float dur = 1.0f / (float)std::max(fps, 1);
            if (playTimer >= dur) {
                playTimer -= dur;
                currentFrame++;
                if (currentFrame >= frameCount()) {
                    if (looping) currentFrame = 0;
                    else { currentFrame = frameCount() - 1; playing = false; }
                }
            }
        }

        void pushUndo() {
            auto* c = current();
            if (!c) return;
            if ((int)undoStack.size() >= MAX_UNDO)
                undoStack.erase(undoStack.begin());
            undoStack.push_back({ currentFrame, c->Pixels() });
            redoStack.clear();
        }

        void undo() {
            auto* c = current();
            if (!c || undoStack.empty()) return;
            redoStack.push_back({ currentFrame, c->Pixels() });
            auto& rec = undoStack.back();
            if (rec.frameIndex >= 0 && rec.frameIndex < frameCount()) {
                currentFrame = rec.frameIndex;
                current()->Pixels() = rec.before;
                current()->UploadTexture();
            }
            undoStack.pop_back();
        }

        void redo() {
            auto* c = current();
            if (!c || redoStack.empty()) return;
            undoStack.push_back({ currentFrame, c->Pixels() });
            auto& rec = redoStack.back();
            if (rec.frameIndex >= 0 && rec.frameIndex < frameCount()) {
                currentFrame = rec.frameIndex;
                current()->Pixels() = rec.before;
                current()->UploadTexture();
            }
            redoStack.pop_back();
        }

        void next() {
            if (currentFrame < frameCount() - 1) currentFrame++;
        }

        void prev() {
            if (currentFrame > 0) currentFrame--;
        }
    };

}