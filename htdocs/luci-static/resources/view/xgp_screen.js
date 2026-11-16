// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 zzzz0317

'use strict';
'require view';
'require dom';
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
	system_info: {
		title: _('System information'),
		description: _('Hostname, system name, system version, and kernel version.')
	},
	system_status: {
		title: _('System status'),
		description: _('Load averages, memory usage, uptime, and local time.')
	},
	network_info: {
		title: _('Network information'),
		description: _('Modem, WAN, and LAN IP addresses, active connections, and online LAN device count.')
	},
	modem_info: {
		title: _('Modem information'),
		description: _('Modem firmware version, temperature, voltage, carrier, network type, band or CQI, and AMBR.')
	},
	modem_signal: {
		title: _('Modem signal'),
		description: _('Values and signal bars for metrics such as RSRP, RSRQ, and SINR.')
	},
	traffic: {
		title: _('Traffic statistics'),
		description: _("Today's and all-time upload and download totals for monitored interfaces.")
	}
};
const pageIds = Object.keys(pages);

function loadKnownChoice(section) {
	const value = form.ListValue.prototype.load.call(this, section);
	return this.keylist.includes(value) ? value : this.default;
}

const TimePicker = ui.AbstractElement.extend({
	__init__(value, options) {
		this.value = value;
		this.options = options;
	},

	render() {
		const [hour, minute] = this.value.split(':');
		const select = (count, value, label, id) => {
			// Keep both controls native so theme dropdowns cannot widen the pair.
			const node = E('select', {
				id,
				style: 'width: 5em; min-width: 0',
				'aria-label': label,
				disabled: this.options.disabled ? true : null
			}, Array.from({ length: count }, (_, index) => {
				const number = String(index).padStart(2, '0');
				return E('option', { value: number }, number);
			}));
			node.value = value;
			return node;
		};

		this.hour = select(24, hour, _('%s (hour)').format(this.options.title), this.options.id);
		this.minute = select(60, minute, _('%s (minute)').format(this.options.title));
		this.node = this.hour;
		dom.bindClassInstance(this.hour, this);
		this.setChangeEvents(this.hour, 'change');
		this.setChangeEvents(this.minute, 'change');

		return E('span', { style: 'display: inline-flex; align-items: center; gap: .5em' }, [
			this.hour, E('span', { 'aria-hidden': 'true' }, ':'), this.minute
		]);
	},

	getValue() {
		return `${this.hour.value}:${this.minute.value}`;
	}
});

const TimeValue = form.Value.extend({
	renderWidget(section, optionIndex, cfgvalue) {
		const value = /^(?:[01][0-9]|2[0-3]):[0-5][0-9]$/.test(cfgvalue) ? cfgvalue : this.default;
		return new TimePicker(value, {
			id: this.cbid(section),
			title: this.title,
			disabled: this.readonly ?? this.map.readonly
		}).render();
	}
});

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

		const mode = s.option(form.ListValue, 'schedule_mode', _('Display schedule'));
		mode.value('always', _('Always on'));
		mode.value('fixed', _('Fixed hours'));
		mode.value('sun', _('On at sunrise, off at sunset'));
		mode.default = 'always';
		mode.rmempty = false;
		mode.retain = true;
		mode.depends('enabled', '1');
		mode.load = loadKnownChoice;

		const onTime = s.option(TimeValue, 'schedule_on', _('On time'));
		const offTime = s.option(TimeValue, 'schedule_off', _('Off time'),
			_('Uses the router’s local time. An earlier off time means the next day.'));
		onTime.default = '08:00';
		offTime.default = '22:00';
		for (const time of [onTime, offTime]) {
			time.rmempty = false;
			time.retain = true;
			time.depends({ enabled: '1', schedule_mode: 'fixed' });
		}

		for (const [name, title, limit] of [
			['latitude', _('Latitude'), 90],
			['longitude', _('Longitude'), 180]
		]) {
			const coordinate = s.option(form.Value, name, title);
			coordinate.rmempty = false;
			coordinate.retain = true;
			coordinate.depends({ enabled: '1', schedule_mode: 'sun' });
			coordinate.validate = (section, value) => {
				const input = value.trim();
				const degrees = Number(input);
				return /^[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)$/.test(input)
					&& degrees >= -limit && degrees <= limit
					|| _('Enter decimal degrees from %d to %d.').format(-limit, limit);
			};
			if (name === 'longitude')
				coordinate.description = '%s <a href="https://www.latlong.net/" target="_blank" rel="noopener noreferrer">%s</a>'
					.format(_('Copy the Latitude and Longitude values for the router’s location.'),
						_('Look up coordinates on LatLong.net'));
		}

		let o = s.option(form.ListValue, 'max_fps', _('Frame rate limit'),
			_('The actual frame rate depends on the transition effect and screen content.'));
		for (const fps of [32, 25, 20, 15])
			o.value(String(fps), _('%d FPS').format(fps));
		o.default = '32';
		o.rmempty = false;
		o.load = loadKnownChoice;

		o = s.option(form.ListValue, 'scroll_type', _('Page transition'));
		o.value('none', _('No animation'));
		o.value('fade', _('Fade in'));
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

		o = s.option(form.Value, 'default_delay', _('Default page duration (ms)'));
		o.datatype = 'and(uinteger,range(0,2147483647))';
		o.default = '5000';
		o.rmempty = false;

		const pageTable = m.section(form.TableSection, 'page', _('Page rotation'),
			_('Enabled pages are shown in the order listed here. Use the drag handles or ↑ and ↓ to reorder them. A duration of 0 uses the default.'));
		pageTable.hidetitle = true;
		pageTable.sectiontitle = section => pages[section].title;
		pageTable.filter = section => Object.hasOwn(pages, section);
		pageTable.cfgsections = function() {
			const sections = form.TableSection.prototype.cfgsections.call(this);
			const order = id => parseInt(this.map.data.get('xgp_screen', id, 'order') ?? (pageIds.indexOf(id) + 1), 10) || 0;
			return sections.sort((a, b) => order(a) - order(b) || pageIds.indexOf(a) - pageIds.indexOf(b));
		};
		pageTable.render = async function() {
			this.sortable = !this.map.readonly;
			const section = await form.TableSection.prototype.render.call(this);
			const body = section.querySelector('tbody');
			const rows = [...body.querySelectorAll('tr[data-sid]')];
			const updateButtons = () => {
				for (const row of rows) {
					row.querySelector('[data-move="up"]').disabled = !row.previousElementSibling;
					row.querySelector('[data-move="down"]').disabled = !row.nextElementSibling;
				}
			};

			for (const row of rows) {
				const page = pages[row.dataset.sid];
				row.querySelector('.cbi-section-table-titles').appendChild(E('div', {
					class: 'cbi-value-description',
					style: 'font-weight: normal; white-space: normal'
				}, page.description));
				if (!this.sortable)
					continue;

				const actions = row.querySelector('.cbi-section-actions > div');
				actions.querySelector('.drag-handle').setAttribute('aria-label', _('Drag to reorder %s').format(page.title));
				for (const [direction, symbol, action] of [
					['up', '↑', _('Move %s up')],
					['down', '↓', _('Move %s down')]
				]) {
					const label = action.format(page.title);
					actions.appendChild(E('button', {
						type: 'button',
						class: 'btn cbi-button cbi-button-neutral',
						style: 'min-width: 2.5em',
						'data-move': direction,
						title: label,
						'aria-label': label,
						click: ev => {
							const down = direction === 'down';
							const sibling = down ? row.nextElementSibling : row.previousElementSibling;
							body.insertBefore(row, down ? sibling.nextElementSibling : sibling);
							this.map.data.move('xgp_screen', row.dataset.sid, sibling.dataset.sid, down);
							updateButtons();
							const target = ev.currentTarget.disabled
								? actions.querySelector(`[data-move="${down ? 'up' : 'down'}"]`) : ev.currentTarget;
							target.focus();
						}
					}, symbol));
				}
			}

			if (this.sortable) {
				section.querySelectorAll('th[data-sortable-row]').forEach(th => th.removeAttribute('data-sortable-row'));
				updateButtons();
				new MutationObserver(updateButtons).observe(body, { childList: true });
			}
			return section;
		};

		const pageEnabled = pageTable.option(form.Flag, 'enabled', _('Enabled'));
		pageEnabled.rmempty = false;
		pageEnabled.default = '1';

		o = pageTable.option(form.Value, 'delay', _('Page duration (ms)'));
		o.datatype = 'and(uinteger,range(0,2147483647))';
		o.default = '5000';
		o.rmempty = false;

		s = m.section(form.NamedSection, 'settings', 'traffic', _('Traffic statistics'));
		s.hidetitle = true;
		const interfaces = s.option(form.DynamicList, 'interfaces', _('Monitored devices'),
			_('Enter a device name or prefix, such as wwan. An empty list uses eth1, wwan, wan and pppoe.'));
		interfaces.placeholder = 'eth1';
		interfaces.datatype = 'maxlength(31)';
		interfaces.validate = (section, value) => !/[\s/:'"\\]/.test(value) || _('Enter a valid device name or prefix.');

		o = s.option(form.Button, '_reset', _('Reset traffic statistics'));
		o.inputtitle = _('Reset statistics');
		o.inputstyle = 'negative';
		o.onclick = confirmTrafficReset;

		m.parse = function() {
			if (enabled.formvalue('main') === '1' && mode.formvalue('main') === 'fixed' &&
			    onTime.formvalue('main') === offTime.formvalue('main'))
				return Promise.reject(new Error(_('Choose different on and off times.')));

			if (enabled.formvalue('main') === '1' && !pageIds.some(section =>
				pageEnabled.formvalue(section) === '1')) {
				return Promise.reject(new Error(_('Enable at least one page before enabling the display.')));
			}

			if (L.toArray(interfaces.formvalue('settings')).length > 16)
				return Promise.reject(new Error(_('Add at most 16 device names or prefixes.')));

			return form.Map.prototype.parse.call(this).then(() => {
				const rows = [...this.root.querySelectorAll('#cbi-xgp_screen-page tbody > tr[data-sid]')];
				const configured = pageTable.cfgsections();
				if (rows.some((row, index) => row.dataset.sid !== configured[index]))
					rows.forEach((row, index) => this.data.set('xgp_screen', row.dataset.sid, 'order', String(index + 1)));
			});
		};

		return m.render();
	}
});
