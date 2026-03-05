#ifndef VCU_CORE_TIMER_H
#define VCU_CORE_TIMER_H


class Timer {
private:
    float duration;
public:
    float time;
    explicit Timer(float duration);
    void reset();
    void count(float deltaTime);
    bool isFinished();
};


#endif //VCU_CORE_TIMER_H
