#ifndef LIBZCOMMON_RANDOM_H
#define LIBZCOMMON_RANDOM_H


class RandomIntGenerator
{
public:
    // 生成[min_value, max_value], 如果传入size() - 1
    int Generate(int min_value, int max_value)
    {
        boost::random::uniform_int_distribution<> dist(min_value, max_value);
        return dist(gen_);
    }

private:
    RandomIntGenerator()
    {
        gen_.seed(static_cast<int32>(time(nullptr)));
    }

    boost::random::mt19937 gen_;

    DECLARE_SINGLETON(RandomIntGenerator);
};

#define RANDOM(a, b) Singleton<RandomIntGenerator>::instance().Generate(a, b)

#endif // LIBZCOMMON_RANDOM_H
