/*
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */
DATABASE DB = "stocks.gdb";

#define number_of_stocks 5

char *event_names [] = { "APOLLO", "DEC", "HP", "IBM", "SUN" };

main() {

	int i;
	double old_prices[number_of_stocks];
             
	READY DB;

	EVENT_INIT PRICE_CHANGE ( "APOLLO", "DEC", "HP", "IBM", "SUN" );

	START_TRANSACTION;

	for (i=0;i<number_of_stocks;i++) {
		FOR S IN STOCKS WITH S.COMPANY = event_names[i]
			old_prices[i]=S.PRICE;
		END_FOR;
		}

	while (1) {


		EVENT_WAIT PRICE_CHANGE;

		COMMIT;

		START_TRANSACTION;

		for (i=0;i<number_of_stocks;i++) {
			if (gds__events[i]) {
				FOR S IN STOCKS WITH S.COMPANY = event_names[i]
					printf ("COMPANY: %s changed!  OLD PRICE: %f  - NEW PRICE: %f\n", 
						S.COMPANY, old_prices[i], S.PRICE);
					old_prices[i]=S.PRICE;
				END_FOR;
				}
			}
		}
}
