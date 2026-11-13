// SPDX-License-Identifier: GPL-3.0-or-later

'use strict';

import { cursor } from 'uci';
import { connect } from 'ubus';

return {
	'luci.xgp_screen': {
		resetTraffic: {
			call: function() {
				// Use a separate UCI cursor so pending LuCI changes stay in the session.
				const ctx = cursor();
				const bus = connect();

				if (!bus || !ctx.set('xgp_screen', 'settings', 'reset', '1') ||
				    !ctx.commit('xgp_screen'))
					return { success: false };

				bus.call('service', 'event', {
					type: 'config.change',
					data: { package: 'xgp_screen' }
				});

				return { success: bus.error() == null };
			}
		}
	}
};
