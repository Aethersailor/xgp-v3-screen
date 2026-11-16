// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 zzzz0317

'use strict';
'require view';
'require form';
'require rpc';
'require ui';

const resetTraffic = rpc.declare({
	object: 'luci.xgp_screen',
	method: 'resetTraffic',
	expect: { success: false },
	reject: true
});

const pages = {
	system_info: _('System information'),
	system_status: _('System status'),
	network_info: _('Network information'),
	modem_info: _('Modem information'),
	modem_signal: _('Modem signal'),
	traffic: _('Traffic statistics')
};

function loadKnownChoice(section) {
	const value = form.ListValue.prototype.load.call(this, section);
	return this.keylist.includes(value) ? value : this.default;
}

function confirmTrafficReset() {
	const title = _('Reset traffic statistics');

	ui.showModal(title, [
		E('p', {}, _("Today's and all-time traffic totals will be cleared the next time the display shows the traffic statistics page.")),
		E('div', { class: 'right' }, [
			E('button', { class: 'cbi-button', click: ui.hideModal }, _('Cancel')),
			' ',
			E('button', {
				class: 'cbi-button cbi-button-negative',
				click: ui.createHandlerFn(this, async function() {
					const success = await resetTraffic().catch(() => false);
					ui.hideModal();
					ui.addNotification(null, E('p', {}, success
						? _('Traffic reset requested.')
						: _('Unable to request a traffic reset.')), success ? 'info' : 'error');
				})
			}, _('Reset statistics'))
		])
	]);
}

return view.extend({
	render() {
		const m = new form.Map('xgp_screen', _('XGP Screen'));
		m.tabbed = true;

		let s = m.section(form.NamedSection, 'main', 'screen', _('General Settings'));
		s.hidetitle = true;

		const enabled = s.option(form.Flag, 'enabled', _('Enable display'),
			_('Disabling the display stops the screen service and turns off the backlight.'));
		enabled.rmempty = false;
		enabled.default = '1';

		let o = s.option(form.ListValue, 'max_fps', _('Frame rate limit'),
			_('The actual frame rate depends on the transition effect and screen content.'));
		for (const fps of [32, 25, 20, 15])
			o.value(String(fps), _('%d FPS').format(fps));
		o.default = '32';
		o.rmempty = false;
		o.load = loadKnownChoice;

		o = s.option(form.ListValue, 'scroll_type', _('Page transition'));
		o.value('none', _('No animation'));
		o.value('fade', _('Fade'));
		o.value('slide_up', _('Slide up'));
		o.value('slide_down', _('Slide down'));
		o.value('slide_left', _('Slide left'));
		o.value('slide_right', _('Slide right'));
		o.value('over_up', _('Cover up'));
		o.value('over_down', _('Cover down'));
		o.value('over_left', _('Cover left'));
		o.value('over_right', _('Cover right'));
		o.default = 'slide_up';
		o.rmempty = false;
		o.load = loadKnownChoice;

		o = s.option(form.Value, 'default_delay', _('Default duration (ms)'));
		o.datatype = 'and(uinteger,range(0,2147483647))';
		o.default = '5000';
		o.rmempty = false;

		s = m.section(form.TableSection, 'page', _('Page rotation'),
			_('Pages are displayed in ascending order. Set a page duration to 0 to use the default duration.'));
		s.hidetitle = true;
		s.sectiontitle = section => pages[section];
		s.filter = section => Object.hasOwn(pages, section);

		const pageEnabled = s.option(form.Flag, 'enabled', _('Enabled'));
		pageEnabled.rmempty = false;
		pageEnabled.default = '1';

		o = s.option(form.Value, 'order', _('Display order'));
		o.datatype = 'and(uinteger,range(0,2147483647))';
		o.rmempty = false;
		o.load = function(section) {
			return form.Value.prototype.load.call(this, section) ?? String(Object.keys(pages).indexOf(section) + 1);
		};

		o = s.option(form.Value, 'delay', _('Duration (ms)'));
		o.datatype = 'and(uinteger,range(0,2147483647))';
		o.default = '5000';
		o.rmempty = false;

		s = m.section(form.NamedSection, 'settings', 'traffic', _('Traffic statistics'));
		s.hidetitle = true;
		const interfaces = s.option(form.DynamicList, 'interfaces', _('Monitored devices'),
			_('Enter a device name or prefix, such as wwan. An empty list uses eth1, wwan, wan and pppoe.'));
		interfaces.placeholder = 'eth1';
		interfaces.datatype = 'maxlength(31)';
		interfaces.validate = (_, value) => !/[\s/:'"\\]/.test(value) || _('Enter a valid device name or prefix.');

		o = s.option(form.Button, '_reset', _('Reset traffic statistics'));
		o.inputtitle = _('Reset statistics');
		o.inputstyle = 'negative';
		o.onclick = confirmTrafficReset;

		m.parse = function() {
			if (enabled.formvalue('main') === '1' && !Object.keys(pages).some(section =>
				pageEnabled.formvalue(section) === '1')) {
				return Promise.reject(new Error(_('Enable at least one page before enabling the display.')));
			}

			if (L.toArray(interfaces.formvalue('settings')).length > 16)
				return Promise.reject(new Error(_('Add at most 16 device names or prefixes.')));

			return form.Map.prototype.parse.call(this);
		};

		return m.render();
	}
});
