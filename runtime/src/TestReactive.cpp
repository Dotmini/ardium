#include "../include/Reactive.h"
#include "../include/Widget.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace Ardium::Titan;

int main() {
    std::cout << "--- [Titan] Reactive State System Test ---\\n";

    // 1. Create State
    State<int> score(0);
    std::cout << "[Logic] State created. Initial Score: " << score.Get() << "\\n";

    // 2. Create UI
    Label scoreLabel("ScoreLabel: ");
    
    // 3. Bind
    std::cout << "[Logic] Binding Label to Score...\\n";
    scoreLabel.Bind(score); // Should print initial value immediately if render logic was inside bind (it isn't, listeners are future)
    // Manually render first frame
    scoreLabel.Render();

    // 4. Update State (Main Thread)
    std::cout << "\\n[Logic] Updating Score to 10...\\n";
    score.Set(10); // Should trigger Label::Render

    std::cout << "\\n[Logic] Updating Score to 20...\\n";
    score.Set(20);

    // 5. Thread Safety Test (Background Thread)
    std::cout << "\\n[Logic] Spawning Background Thread for updates...\\n";
    std::thread bgThread([&score]() {
        for (int i = 1; i <= 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            int newScore = 20 + i * 5;
            std::cout << "[Thread] Setting Score to " << newScore << "...\\n";
            score.Set(newScore);
        }
    });

    bgThread.join();

    std::cout << "\\n--- [Titan] Test Complete ---\\n";
    return 0;
}
