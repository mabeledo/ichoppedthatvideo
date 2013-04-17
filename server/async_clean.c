/* File: async_clean.c
 * Author: mabeledo (m.a.abeledo.garcia@members.fsf)
 * License: GPLv3
 */ 


/* check_video_alloc()
 * 
 * Synchronous, threaded memory checking function.
 * */
void *
check_video_alloc		(void * arg)
{
	int i, videos_num_old, marked_num, * marked;
	int64_t period_total, period_avg;
	struct timespec delay = {CHECK_TIME, 0};
	Video * cur_video;
	
	videos_num_old = videos_num;
	
	/* Endless loop to check unused allocated videos. */
	while (TRUE)
	{
		/* Delay checking 'delay' seconds. */
		clock_nanosleep(CLOCK_REALTIME, 0, delay, NULL);
		marked = malloc(videos_num * sizeof(int));
		marked_num = 0;

		pthread_mutex_lock(&stream_lock);
		period_avg = (period_total + (videos_num - videos_num_old)) / videos_num;
		
		/* Update information about video use. */
		for (i = 0; i < videos_num; i++)
		{
			if (((videos[i]->period_count += (videos[i]->request_count > 0) ? 1 : -1) < 0) ||
				(videos[i]->period_count < period_avg))
			{
				marked[marked_num++] = i;
			}
			else
			{
				period_total += videos[i]->period_count;
			}
			
			videos[i]->request_count = 0;
		}

		/* Delete videos from memory if and only if:
		 *  - Memory limit is reached.
		 *  - Video request count is negative.
		 * */
		if ((marked_num > 0) &&
			(mem_used > mem_limit))
		{
			
		
			
			videos_num_old = videos_num;
		}
		
		pthread_mutex_unlock(&stream_lock);
		free(marked);
	}
	
	return (SUCCESS);
}
