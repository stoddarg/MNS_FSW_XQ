/*
 * process_data.c
 *
 *  Created on: May 9, 2018
 *      Author: gstoddard
 */

#include "process_data.h"

//File Scope Variables and Buffers
static int evt_iter;										//event buffer iterator
static const GENERAL_EVENT_TYPE evtEmptyStruct;				//use this to reset the holder struct each iteration
static GENERAL_EVENT_TYPE event_buffer[EVENT_BUFFER_SIZE];	//buffer to store events //2048 * 8 bytes = 16384 bytes
static unsigned int m_neutron_counts;						//total neutron counts
static unsigned int m_event_number;							//event number holder
static unsigned int m_first_event_time_FPGA;				//the first event time which needs to be written into every data product header
/*
 * Helper function to allow external functions to grab the EVTs buffer and write it to SD
 */
GENERAL_EVENT_TYPE * GetEVTsBufferAddress( void )
{
	return event_buffer;
}


void ResetEVTsBuffer( void )
{
	memset(event_buffer, '\0', sizeof(event_buffer));
	return;
}

void ResetEVTsIterator( void )
{
	evt_iter = 0;
	return;
}


unsigned int GetFirstEventTime( void )
{
	return m_first_event_time_FPGA;
}


/*
 * This function will be called after we read in a buffer of valid data from the FPGA.
 *  Here is where the data stream from the FPGA is scanned for events and each event
 *  is processed to pull the PSD and energy information out. We identify it the event
 *  is within the current 1 second CPS interval, as well as bin the events into a
 *  2-D histogram which is reported at the end of a run.
 *
 * @param	A pointer to the data buffer
 *
 * @return	SUCCESS/FAILURE
 */
int ProcessData( unsigned int * data_raw )
{
	bool valid_event = FALSE;	//Unused (4/8/2019) //could use this to check for reset request //otherwise probably delete
	int iter = 0;
	int m_ret = 0;	//for 2DH tallies
	int m_events_processed = 0;
	int m_neutron_detected = 0;
	unsigned int m_x_bin_number = 0;
	unsigned int m_y_bin_number = 0;
	unsigned int m_tagging_bit = 0;
	unsigned int num_bytes_written = 0;
	unsigned int m_total_events_holder = 0;
	unsigned int m_pmt_ID_holder = 0;
	unsigned int m_FPGA_time_holder = 0;
	unsigned int m_bad_event = 0;
	double m_baseline_int = 0.0;
	double m_short_int = 0.0;
	double m_long_int = 0.0;
	double m_full_int = 0.0;
	double bl_avg = 0.0;
	double bl1 = 0.0;
	double bl2 = 0.0;
	double bl3 = 0.0;
	double bl4 = 0.0;
	double si = 0.0;
	double li = 0.0;
	double fi = 0.0;
	double psd = 0.0;
	double energy = 0.0;
	FRESULT f_res = FR_OK;
	GENERAL_EVENT_TYPE event_holder = evtEmptyStruct;

	//get the integration times
	m_baseline_int = (double)GetBaselineInt();
	m_short_int = (double)GetShortInt();
	m_long_int = (double)GetLongInt();
	m_full_int = (double)GetFullInt();

	FIL *cpsDataFile = GetCPSFilePointer();
	if (cpsDataFile == NULL)
	{
		//TODO: handle error with pointer
		xil_printf("error getting FIL pointer 6\n");
	}

	while(iter < DATA_BUFFER_SIZE)
	{
		event_holder = evtEmptyStruct;	//reset event structure
		m_neutron_detected = 0;			//reset the neutron detected value

		switch(data_raw[iter])
		{
		case 111111: //this is the data event case
			while(data_raw[iter+1] == 111111 && iter < (DATA_BUFFER_SIZE - EVT_EVENT_SIZE))//handles any number of 111111's in succession
			{
				iter++;
			}
			if(iter >= (DATA_BUFFER_SIZE - 7))	//if we are at the top of the buffer, break out to avoid bad indexing
				break;
			if(data_raw[iter+1] >= cpsGetCurrentTime())	//time must be the same or increasing
			{
				if(data_raw[iter+2] >= m_neutron_counts)	//counts must be the same or increasing
				{
					if(data_raw[iter+3] > m_event_number)
					{
						if((data_raw[iter+4] < data_raw[iter+5]) && (data_raw[iter+5] < data_raw[iter+6]) && (data_raw[iter+6] < data_raw[iter+7]))
						{
							valid_event = TRUE;
							//if the first event time has not been recorded, then set one //this allows us to function without a false event
							if(cpsGetFirstEventTime() == 0)
								cpsSetFirstEventTime(data_raw[iter+1]);
							//loop recording the CPS events until we don't need to
							while(cpsCheckTime(data_raw[iter+1]) == TRUE)
							{
								f_res = f_write(cpsDataFile, (char *)cpsGetEvent(), CPS_EVENT_SIZE, &num_bytes_written);
								if(f_res != FR_OK || num_bytes_written != CPS_EVENT_SIZE)
								{
									//TODO:handle error with writing
									xil_printf("error writing 4\n");
								}
								f_res = f_sync(cpsDataFile);
								if(f_res != FR_OK || num_bytes_written != CPS_EVENT_SIZE)
								{
									//TODO:handle error with writing
									xil_printf("error writing 5\n");
								}
								//reset the neutron counts for the CPS data product
								CPSResetCounts();
							}

							event_holder.field0 = 0xFF;	//event ID is 0xFF
							m_pmt_ID_holder = data_raw[iter+3] & 0x0F;
							switch(m_pmt_ID_holder)
							{
							case PMT_ID_0:
								event_holder.field1 |= 0x10; //PMT 0
								break;
							case PMT_ID_1:
								event_holder.field1 |= 0x20; //PMT 1
								break;
							case PMT_ID_2:
								event_holder.field1 |= 0x40; //PMT 2
								break;
							case PMT_ID_3:
								event_holder.field1 |= 0x80; //PMT 3
								break;
							default:
								//handles bad/multi-hit PMT IDs
								event_holder.field1 |= (m_pmt_ID_holder << 4); //shift by 4 to get this to fit correctly
								break;
							}
							m_total_events_holder = data_raw[iter+2] & 0xFFF;	//mask the upper bits we don't care about
							event_holder.field1 |= (unsigned char)(m_total_events_holder >> 8);
							event_holder.field2 |= (unsigned char)(m_total_events_holder & 0xFF);

							si = 0.0;	li = 0.0;	fi = 0.0;	psd = 0.0;	energy = 0.0;
							bl4 = bl3; bl3 = bl2; bl2 = bl1;
							bl1 = (double)data_raw[iter+4] / (16.0 * m_baseline_int);
							if(bl4 == 0.0)
								bl_avg = bl1;
							else
								bl_avg = (bl4 + bl3 + bl2 + bl1) / 4.0;
							si = ((double)data_raw[iter+5]) / (16.0) - (bl_avg * m_short_int);
							li = ((double)data_raw[iter+6]) / (16.0) - (bl_avg * m_long_int);
							fi = ((double)data_raw[iter+7]) / (16.0) - (bl_avg * m_full_int);
							energy = fi;

							//li, si must be positive, li greater than si (ensures positive psd values and li != si)
							if( li > 0 && si > 0 && li > si) //TODO: how much should we test here? //si != 0, li > si, si > 0 ?
								psd = si / (li - si);
							else
							{
								//TODO: PSD value not good
								psd = 1.999;	//set to highest good bin
								m_bad_event++;
							}

							m_neutron_detected = CPSUpdateTallies(energy, psd);
							IncNeutronTotal(m_neutron_detected);	//increment the neutron total by 1? TODO: check the return here and make sure it has increased?
							if(m_neutron_detected == 1)
								m_tagging_bit = 1;
							else
								m_tagging_bit = 0;

							//add the energy and PSD tallies to the correct histogram
							m_ret = Tally2DH(energy, psd, m_pmt_ID_holder);
							if(m_ret == CMD_FAILURE)
							{
								//handle error in tallying the event into the 2DH
								//TODO: identify what can go wrong and handle a bad tally
							}
							m_x_bin_number = Get2DHArrayIndexX();
							m_y_bin_number = Get2DHArrayIndexY();
							event_holder.field3 |= (unsigned char)(m_x_bin_number >> 1);
							event_holder.field4 |= (unsigned char)((m_x_bin_number & 0x01) << 7);
							event_holder.field4 |= (unsigned char)(m_y_bin_number << 1);
							event_holder.field4 |= (unsigned char)(m_tagging_bit);
							m_FPGA_time_holder = ((data_raw[iter+1] & 0xFFFFFF00) >> 8);	//mask and shift off the bottom 8 bits
							event_holder.field5 = (unsigned char)(m_FPGA_time_holder >> 16);
							event_holder.field6 = (unsigned char)(m_FPGA_time_holder >> 8);
							event_holder.field7 = (unsigned char)(m_FPGA_time_holder);
							event_buffer[evt_iter] = event_holder;
							evt_iter++;
							iter += 8;
							m_events_processed++;
						}
						else
							valid_event = FALSE;
					}
					else
						valid_event = FALSE;
				}
				else
					valid_event = FALSE;
			}
			else
				valid_event = FALSE;

			if(valid_event == FALSE)
				iter++;
			break;
		case 2147594759:	//this is a false event
			if(iter >= DATA_BUFFER_SIZE - 9 )	//meant to protect from writing above array indices...
			{
				iter++;
				break;	//skip out early
			}
			if( data_raw[iter + 1] == 2147594759 && data_raw[iter + 9] == 111111)
			{
				cpsSetFirstEventTime(data_raw[iter + 2]);
				m_first_event_time_FPGA = data_raw[iter + 2];

				event_holder.field0 = 0xDD;
				event_holder.field1 = 0xDD;
				event_holder.field2 = 0xDD;
				event_holder.field3 = 0xDD;
				event_holder.field4 = 0x00 | (data_raw[iter + 2] >> 30);
				event_holder.field5 = data_raw[iter + 2] >> 22;
				event_holder.field6 = data_raw[iter + 2] >> 14;
				event_holder.field7 = data_raw[iter + 2] >> 6;

				event_buffer[evt_iter] = event_holder;
				evt_iter++;
				iter += 8; //shouldn't this be + 9?
				m_events_processed++;
			}
			else
				iter++;

			break;
		default:
			//this indicates that we miscounted our place in the buffer somewhere
			//or there is junk in the buffer in the middle of an event
			//TODO: handle a bad event or junk in the buffer
			iter++;	//move past it, at least, we can sync back up by finding the 111111 again
			break;
		}//END OF SWITCH ON RAW DATA

		if(iter > (DATA_BUFFER_SIZE - EVT_EVENT_SIZE))	//will read past the array if iter goes above
			break;
		if(evt_iter >= EVENT_BUFFER_SIZE)	//we have run out of open events in the buffer
			break;
		if(m_events_processed >= VALID_BUFFER_SIZE)	//we have processed every event in the buffer (max of 512)
			break;
		//TODO: fully error check the buffering here
		//2-15, anything else?
	}//END OF WHILE

	//TODO: give this return value a meaning
	return 0;
}
