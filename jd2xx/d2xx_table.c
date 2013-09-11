/*
 * Dummy file implementing a libd2xx_table shared library used by D2XX
 * to verify Vendor/Product Id tuples.
 * The D2XX library expects to find it in the LD_LIBRARY_PATH and loads
 * it dynamically.
 * It must be called "libd2xx_table.so".
 *
 * Otherwise D2XX on Linux will only handle devices with a few FTDI
 * product Ids.
 * Also the FT_SetVIDPID() only allows you to set a *single*
 * exception.
 */

/*
 * int lib_check_device(int vendor, int product)
 *
 * Description: Check the VID and PID against our table
 * Arguments: vendor (device to check VID), product (device to check PID)
 * Return: 0 if no match, 1 if match
 */
int
lib_check_device(int vendor, int product)
{
	return vendor == 0x0403;
}

