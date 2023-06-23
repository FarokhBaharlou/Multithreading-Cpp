#include "Task.h"
#include "Preassigned.h"
#include "Queued.h"

int main()
{
    Dataset data;
    data = GenerateDatasetStacked();
    //data = GenerateDatasetEven();
    //data = GenerateDatasetRandom();

    // return Pre::DoTest(std::move(data));
    return Que::DoTest(std::move(data));
}