#include "misc.h"

static void _Sort(i16 *arr, int n)
{
	for (int i = 1; i < n; i++) {
		i16 key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}




// ====== interface ======
i16 GetMedian_16(i16 *arr, int n)
{
	_Sort(arr, n);
	return arr[n>>1];
}