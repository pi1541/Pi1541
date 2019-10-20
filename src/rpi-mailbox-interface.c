#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "rpiHardware.h"
#include "rpi-mailbox.h"
#include "rpi-mailbox-interface.h"
#include "cache.h"

/* Make sure the property tag buffer is aligned to a 16-byte boundary because
   we only have 28-bits available in the property interface protocol to pass
   the address of the buffer to the VC. */
static int *pt = (int *)UNCACHED_MEM_BASE;
static int pt_index ;

//#define PRINT_PROP_DEBUG 1

void RPI_PropertyInit( void )
{
    //memset(pt, 0, sizeof(pt));

    /* Fill in the size on-the-fly */
    pt[PT_OSIZE] = 12;

    /* Process request (All other values are reserved!) */
    pt[PT_OREQUEST_OR_RESPONSE] = 0;

    /* First available data slot */
    pt_index = 2;

    /* NULL tag to terminate tag list */
    pt[pt_index] = 0;
}

/**
    @brief Add a property tag to the current tag list. Data can be included. All data is uint32_t
    @param tag
*/
void RPI_PropertyAddTag( rpi_mailbox_tag_t tag, ... )
{
	int* ptr;
	int value;
	int index;

    va_list vl;
    va_start( vl, tag );

    pt[pt_index++] = tag;

    switch( tag )
    {
        case TAG_GET_FIRMWARE_VERSION:
        case TAG_GET_BOARD_MODEL:
        case TAG_GET_BOARD_REVISION:
        case TAG_GET_BOARD_MAC_ADDRESS:
        case TAG_GET_BOARD_SERIAL:
        case TAG_GET_ARM_MEMORY:
        case TAG_GET_VC_MEMORY:
        case TAG_GET_DMA_CHANNELS:
            /* Provide an 8-byte buffer for the response */
            pt[pt_index++] = 8;
            pt[pt_index++] = 0; /* Request */
            pt_index += 2;
            break;

        case TAG_GET_CLOCKS:
        case TAG_GET_COMMAND_LINE:
            /* Provide a 1024-byte buffer */
            pt[pt_index++] = PROP_SIZE;
            pt[pt_index++] = 0; /* Request */
            pt_index += PROP_SIZE >> 2;
            break;

        case TAG_GET_CLOCK_RATE:
        case TAG_GET_MAX_CLOCK_RATE:
        case TAG_GET_MIN_CLOCK_RATE:
        case TAG_GET_TURBO:
        case TAG_GET_TEMPERATURE:
        case TAG_GET_MAX_TEMPERATURE:
        case TAG_GET_VOLTAGE:
        case TAG_GET_MIN_VOLTAGE:
        case TAG_GET_MAX_VOLTAGE:
            pt[pt_index++] = 8;
            pt[pt_index++] = 0; /* Request */
            pt[pt_index++] = va_arg( vl, int ); /* ClockID */
            pt_index += 1;
            break;

        case TAG_EXECUTE_CODE:
            pt[pt_index++] = 28;
            pt[pt_index++] = 0; /* Request */
            pt[pt_index++] = va_arg( vl, int ); // Function pointer
            pt[pt_index++] = va_arg( vl, int ); // R0
            pt[pt_index++] = va_arg( vl, int ); // R1
            pt[pt_index++] = va_arg( vl, int ); // R2
            pt[pt_index++] = va_arg( vl, int ); // R3
            pt[pt_index++] = va_arg( vl, int ); // R4
            pt[pt_index++] = va_arg( vl, int ); // R5
            break;   

        case TAG_ALLOCATE_BUFFER:
            pt[pt_index++] = 8;
            pt[pt_index++] = 0; /* Request */
            pt[pt_index++] = va_arg( vl, int );
            pt_index += 1;
            break;

        case TAG_GET_PHYSICAL_SIZE:
        case TAG_SET_PHYSICAL_SIZE:
        case TAG_TEST_PHYSICAL_SIZE:
        case TAG_GET_VIRTUAL_SIZE:
        case TAG_SET_VIRTUAL_SIZE:
        case TAG_TEST_VIRTUAL_SIZE:
        case TAG_GET_VIRTUAL_OFFSET:
        case TAG_SET_VIRTUAL_OFFSET:
            pt[pt_index++] = 8;
            pt[pt_index++] = 0; /* Request */

            if( ( tag == TAG_SET_PHYSICAL_SIZE ) ||
                ( tag == TAG_SET_VIRTUAL_SIZE ) ||
                ( tag == TAG_SET_VIRTUAL_OFFSET ) ||
                ( tag == TAG_TEST_PHYSICAL_SIZE ) ||
                ( tag == TAG_TEST_VIRTUAL_SIZE ) )
            {
                pt[pt_index++] = va_arg( vl, int ); /* Width */
                pt[pt_index++] = va_arg( vl, int ); /* Height */
            }
            else
            {
                pt_index += 2;
            }
            break;


        case TAG_GET_ALPHA_MODE:
        case TAG_SET_ALPHA_MODE:
        case TAG_GET_DEPTH:
        case TAG_SET_DEPTH:
        case TAG_GET_PIXEL_ORDER:
        case TAG_SET_PIXEL_ORDER:
        case TAG_GET_PITCH:
            pt[pt_index++] = 4;
            pt[pt_index++] = 0; /* Request */

            if( ( tag == TAG_SET_DEPTH ) ||
                ( tag == TAG_SET_PIXEL_ORDER ) ||
                ( tag == TAG_SET_ALPHA_MODE ) )
            {
                /* Colour Depth, bits-per-pixel \ Pixel Order State */
                pt[pt_index++] = va_arg( vl, int );
            }
            else
            {
                pt_index += 1;
            }
            break;

        case TAG_GET_OVERSCAN:
        case TAG_SET_OVERSCAN:
            pt[pt_index++] = 16;
            pt[pt_index++] = 0; /* Request */

            if( ( tag == TAG_SET_OVERSCAN ) )
            {
                pt[pt_index++] = va_arg( vl, int ); /* Top pixels */
                pt[pt_index++] = va_arg( vl, int ); /* Bottom pixels */
                pt[pt_index++] = va_arg( vl, int ); /* Left pixels */
                pt[pt_index++] = va_arg( vl, int ); /* Right pixels */
            }
            else
            {
                pt_index += 4;
            }
            break;

		case TAG_SET_POWER_STATE:
		case TAG_GET_POWER_STATE:
			pt[pt_index++] = 8;
			pt[pt_index++] = 0; /* Request */
			if ((tag == TAG_SET_POWER_STATE))
			{
				pt[pt_index++] = va_arg(vl, int); // deviceID
				pt[pt_index++] = va_arg(vl, int); // state
			}
			else
			{
				pt_index += 2;
			}
			pt_index += 1;

			break;

		case TAG_SET_PALETTE:
			ptr = va_arg(vl, int*);
			value = ptr[1];				// number of palette entries
			pt[pt_index++] = 8 + value * 4;
			pt[pt_index++] = 0; /* Request */
			pt[pt_index++] = *ptr++;	// first palette index
//			value = *ptr++;				// number of palette entries
			ptr++;
			pt[pt_index++] = value;
			{
				for (index = 0; index < value; ++index)
				{
					pt[pt_index++] = *ptr++;
				}
			}
			break;


			//Set the PWR led signal to output mode, allowing to control it : rpi3 - gpiovirtbuf c 135 1 0 0 0 0
			//Set the PWR led off : rpi3 - gpiovirtbuf s 135 0

			//GPIO 134 is the camera LED GPIO
			//GPIO 133 is the camera power GPIO

		case TAG_GET_GPIOVIRTBUF:
		case TAG_GET_TOUCHBUF:
			pt[pt_index++] = 4;
			pt[pt_index++] = 0; /* Request */
			pt_index += 1;
			break;

        default:
            /* Unsupported tags, just remove the tag from the list */
            pt_index--;
            break;
    }

    /* Make sure the tags are 0 terminated to end the list and update the buffer size */
    pt[pt_index] = 0;

    va_end( vl );
}



int RPI_PropertyProcess( void )
{
    int result;

#if( PRINT_PROP_DEBUG == 1 )
    int i;
    LOG_INFO( "%s Length: %d\r\n", __func__, pt[PT_OSIZE] );
#endif
    /* Fill in the size of the buffer */
    pt[PT_OSIZE] = ( pt_index + 1 ) << 2;
    pt[PT_OREQUEST_OR_RESPONSE] = 0;

#if( PRINT_PROP_DEBUG == 1 )
    for( i = 0; i < (pt[PT_OSIZE] >> 2); i++ )
        LOG_INFO( "Request: %3d %8.8X\r\n", i, pt[i] );
#endif
	RPI_Mailbox0Write(MB0_TAGS_ARM_TO_VC, (unsigned int)pt);

    result = RPI_Mailbox0Read( MB0_TAGS_ARM_TO_VC );

#if( PRINT_PROP_DEBUG == 1 )
    for( i = 0; i < (pt[PT_OSIZE] >> 2); i++ )
        LOG_INFO( "Response: %3d %8.8X\r\n", i, pt[i] );
#endif
	return result;
}

void RPI_PropertyProcessNoCheck( void )
{
#if( PRINT_PROP_DEBUG == 1 )
    int i;
    LOG_INFO( "%s Length: %d\r\n", __func__, pt[PT_OSIZE] );
#endif
    /* Fill in the size of the buffer */
    pt[PT_OSIZE] = ( pt_index + 1 ) << 2;
    pt[PT_OREQUEST_OR_RESPONSE] = 0;

#if( PRINT_PROP_DEBUG == 1 )
    for( i = 0; i < (pt[PT_OSIZE] >> 2); i++ )
        LOG_INFO( "Request: %3d %8.8X\r\n", i, pt[i] );
#endif
    RPI_Mailbox0Write( MB0_TAGS_ARM_TO_VC, (unsigned int)pt );
}

rpi_mailbox_property_t* RPI_PropertyGet( rpi_mailbox_tag_t tag)
{
    static rpi_mailbox_property_t property;
    int* tag_buffer = NULL;

    property.tag = tag;

    /* Get the tag from the buffer. Start at the first tag position  */
    int index = 2;

    while( index < ( pt[PT_OSIZE] >> 2 ) )
    {
        /* DEBUG_LOG( "Test Tag: [%d] %8.8X\r\n", index, pt[index] ); */
        if( pt[index] == tag )
        {
           tag_buffer = &pt[index];
           break;
        }

        /* Progress to the next tag if we haven't yet discovered the tag */
        index += ( pt[index + 1] >> 2 ) + 3;
    }

    /* Return NULL of the property tag cannot be found in the buffer */
    if( tag_buffer == NULL )
        return NULL;

    /* Return the required data */
    property.byte_length = tag_buffer[T_ORESPONSE] & 0xFFFF;
    memcpy( property.data.buffer_8, &tag_buffer[T_OVALUE], property.byte_length );

    return &property;
}
