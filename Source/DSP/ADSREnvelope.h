#pragma once

class ADSREnvelope
{
public:
    enum class State { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sr) { sampleRate_ = sr; }

    void setParameters(float attackMs, float decayMs, float sustainLevel, float releaseMs)
    {
        attackSamples_  = msToSamples(attackMs);
        decaySamples_   = msToSamples(decayMs);
        sustainLevel_   = sustainLevel;
        releaseSamples_ = msToSamples(releaseMs);
    }

    void noteOn()
    {
        state_ = State::Attack;
        if (attackSamples_ <= 0.0f)
        {
            level_ = 1.0f;
            enterDecay();
        }
    }

    void noteOff()
    {
        if (state_ == State::Idle) return;
        state_ = State::Release;
        if (releaseSamples_ <= 0.0f)
        {
            level_ = 0.0f;
            state_ = State::Idle;
        }
        else
        {
            releaseRate_ = level_ / releaseSamples_;
        }
    }

    void reset()
    {
        state_ = State::Idle;
        level_ = 0.0f;
    }

    float getNextSample()
    {
        switch (state_)
        {
            case State::Idle:
                return 0.0f;

            case State::Attack:
                level_ += 1.0f / attackSamples_;
                if (level_ >= 1.0f)
                {
                    level_ = 1.0f;
                    enterDecay();
                }
                return level_;

            case State::Decay:
            {
                float decayRate = (1.0f - sustainLevel_) / decaySamples_;
                level_ -= decayRate;
                if (level_ <= sustainLevel_)
                {
                    level_ = sustainLevel_;
                    state_ = State::Sustain;
                }
                return level_;
            }

            case State::Sustain:
                return level_;

            case State::Release:
                level_ -= releaseRate_;
                if (level_ <= 0.0f)
                {
                    level_ = 0.0f;
                    state_ = State::Idle;
                }
                return level_;
        }
        return 0.0f;
    }

    State getState() const { return state_; }
    bool isActive() const { return state_ != State::Idle; }

private:
    State state_ = State::Idle;
    double sampleRate_ = 44100.0;
    float level_ = 0.0f;

    float attackSamples_  = 0.0f;
    float decaySamples_   = 0.0f;
    float sustainLevel_   = 1.0f;
    float releaseSamples_ = 0.0f;
    float releaseRate_    = 0.0f;

    float msToSamples(float ms) const
    {
        return static_cast<float>(ms * 0.001 * sampleRate_);
    }

    void enterDecay()
    {
        if (decaySamples_ <= 0.0f)
        {
            level_ = sustainLevel_;
            state_ = State::Sustain;
        }
        else
        {
            state_ = State::Decay;
        }
    }
};
