// clinic-views.jsx — section views for MedFlow Clinic admin
// Exports all view components + modals to window

const API = 'http://localhost:8420/api';
const apiFetch = async (path, opts = {}) => {
  const res = await fetch(API + path, opts);
  const json = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(json.error || `HTTP ${res.status}`);
  return json;
};
const apiGet = p => apiFetch(p);
const apiPost = (p, b) => apiFetch(p, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(b) });
const apiPatch = (p, b) => apiFetch(p, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(b) });
const apiDelete = p => apiFetch(p, { method: 'DELETE' });

// ── tiny helpers ──────────────────────────────────────────────────────────────
const DAYS = ['','Пн','Вт','Ср','Чт','Пт','Сб','Вс'];
const STATUS_LABEL = ['Запланирована','Завершена','Отменена'];
const STATUS_TAG   = ['ok','info','err'];
const GENDER_LABEL = ['Жен.','Муж.','—'];

function fmtDate(s) {
  if (!s) return '—';
  const [y,m,d] = s.split('-');
  return `${d}.${m}.${y}`;
}
function ageOf(b, today) {
  if (!b || !today) return '';
  const now = new Date(today + 'T00:00:00');
  const bd = new Date(b);
  let a = now.getFullYear() - bd.getFullYear();
  if (now < new Date(now.getFullYear(), bd.getMonth(), bd.getDate())) a--;
  return a + ' лет';
}
function initials(p) {
  if (!p) return '??';
  return ((p.last_name||'')[0]||'') + ((p.first_name||'')[0]||'');
}
function fullName(p) {
  if (!p) return '—';
  return [p.last_name, p.first_name, p.middle_name].filter(Boolean).join(' ');
}
function shortName(p) {
  if (!p) return '—';
  const i = n => n ? n[0]+'.' : '';
  return `${p.last_name||''} ${i(p.first_name)}${i(p.middle_name)}`.trim();
}

// ── shared micro-components ───────────────────────────────────────────────────
function Av({ p, size = 28, fs = 10 }) {
  return <span className="avatar" style={{ width: size, height: size, fontSize: fs }}>{initials(p)}</span>;
}
function Tag({ type, children }) {
  return <span className={`tag dot ${type || ''}`}>{children}</span>;
}
function Spinner() {
  return <div style={{ padding: 40, textAlign: 'center', color: 'var(--ink-3)', fontSize: 13 }}>Загрузка…</div>;
}
function Empty({ text }) {
  return <div style={{ padding: 40, textAlign: 'center', color: 'var(--ink-3)', fontSize: 13 }}>{text}</div>;
}
function Modal({ title, onClose, children, width = 480 }) {
  React.useEffect(() => {
    const h = e => e.key === 'Escape' && onClose();
    window.addEventListener('keydown', h);
    return () => window.removeEventListener('keydown', h);
  }, []);
  return (
    <div className="modal-backdrop" onClick={e => e.target === e.currentTarget && onClose()}>
      <div className="modal-box" style={{ width }}>
        <div className="modal-head">
          <span className="modal-title">{title}</span>
          <button className="icon-btn" onClick={onClose}>
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"><path d="M18 6L6 18M6 6l12 12"/></svg>
          </button>
        </div>
        <div className="modal-body">{children}</div>
      </div>
    </div>
  );
}
function ConfirmModal({ text, onConfirm, onClose }) {
  return (
    <Modal title="Подтверждение" onClose={onClose} width={360}>
      <p style={{ color: 'var(--ink-2)', fontSize: 14, marginBottom: 20 }}>{text}</p>
      <div style={{ display: 'flex', gap: 8, justifyContent: 'flex-end' }}>
        <button className="btn ghost" onClick={onClose}>Отмена</button>
        <button className="btn danger" onClick={onConfirm}>Удалить</button>
      </div>
    </Modal>
  );
}
function Field({ label, hint, children }) {
  return (
    <div className="field">
      <label>{label}</label>
      {children}
      {hint && <div className="hint">{hint}</div>}
    </div>
  );
}
function Input(props) {
  return <input className="input-el" {...props} />;
}
function Select({ children, ...props }) {
  return <select className="input-el" {...props}>{children}</select>;
}

// ── DASHBOARD VIEW ────────────────────────────────────────────────────────────
function DashboardView({ clinicId, clinic, patients, doctors, rooms, appointments, schedules, specs, today, onNavigate, onNewAppt, toast }) {
  const todayAppts = appointments.filter(a => a.scheduledAt && a.scheduledAt.startsWith(today) && a.status !== 2)
    .sort((a, b) => a.scheduledAt.localeCompare(b.scheduledAt));
  const doctorMap = Object.fromEntries(doctors.map(d => [d.id, d]));
  const patientMap = Object.fromEntries(patients.map(p => [p.id, p]));
  const specMap = Object.fromEntries(specs.map(s => [s.id, s]));

  const stats = [
    { label: 'Записей сегодня', value: todayAppts.length, hi: true },
    { label: 'Пациентов', value: patients.length },
    { label: 'Врачей', value: doctors.length },
    { label: 'Кабинетов', value: rooms.length },
  ];

  // mini calendar for the current month
  const [yStr, mStr] = today.split('-');
  const year  = parseInt(yStr, 10);
  const month = parseInt(mStr, 10);            // 1..12
  const todayDay = parseInt(today.slice(8,10), 10);
  const firstDow = (() => {                    // Mon=1..Sun=7
    const j = new Date(`${yStr}-${mStr}-01T00:00:00`).getDay();
    return j === 0 ? 7 : j;
  })();
  const daysInMonth = new Date(year, month, 0).getDate();
  const prevMonthDays = new Date(year, month - 1, 0).getDate();
  const calDays = [];
  for (let i = firstDow - 1; i > 0; i--) calDays.push({ n: prevMonthDays - i + 1, mute: true });
  const apptDates = new Set(appointments.filter(a=>a.status!==2).map(a=>a.scheduledAt&&a.scheduledAt.slice(0,10)));
  for (let d = 1; d <= daysInMonth; d++) {
    const ds = `${yStr}-${mStr}-${String(d).padStart(2,'0')}`;
    calDays.push({ n: d, today: d === todayDay, has: apptDates.has(ds) });
  }
  let fill = 1;
  while (calDays.length < 42 && calDays.length % 7 !== 0) calDays.push({ n: fill++, mute: true });
  while (calDays.length < 35) calDays.push({ n: fill++, mute: true });

  const DOW_LONG  = ['Воскресенье','Понедельник','Вторник','Среда','Четверг','Пятница','Суббота'];
  const MONTHS_NOM = ['Январь','Февраль','Март','Апрель','Май','Июнь','Июль','Август','Сентябрь','Октябрь','Ноябрь','Декабрь'];
  const todayDateObj = new Date(today + 'T00:00:00');
  const subTitle = `${DOW_LONG[todayDateObj.getDay()]}, ${todayDay} ${MONTH_GEN[month - 1]} ${year}`;
  const monthTitle = `${MONTHS_NOM[month - 1]} ${year}`;

  return (
    <div className="main">
      <div className="page-head">
        <div>
          <h1>Дашборд</h1>
          <div className="page-sub">{subTitle} · {clinic?.name || '—'}</div>
        </div>
        <div className="head-actions">
          <button className="btn primary" onClick={onNewAppt}>+ Новая запись</button>
        </div>
      </div>

      <div className="stat-grid" style={{ gridTemplateColumns: 'repeat(4,1fr)' }}>
        {stats.map(s => (
          <div key={s.label} className={`stat${s.hi ? ' highlight' : ''}`}>
            <span className="label">{s.label}</span>
            <div className="num">{s.value}</div>
          </div>
        ))}
      </div>

      <div className="two-col">
        <div className="card">
          <div className="card-head">
            <h3>Записи на сегодня</h3>
            <span className="head-note">{today}</span>
            <div className="tail">
              <button className="btn ghost" style={{ padding: '4px 10px' }} onClick={() => onNavigate('calendar')}>Расписание →</button>
            </div>
          </div>
          {todayAppts.length === 0
            ? <Empty text="Записей на сегодня нет" />
            : (
              <table className="list">
                <thead><tr><th>Время</th><th>Пациент</th><th>Врач</th><th>Статус</th></tr></thead>
                <tbody>
                  {todayAppts.slice(0, 8).map(a => {
                    const doc = doctorMap[a.doctorId];
                    const pat = patientMap[a.patientId];
                    return (
                      <tr key={a.id}>
                        <td className="mono">{a.scheduledAt?.slice(11)}</td>
                        <td className="cell-name"><Av p={pat} />{shortName(pat)}</td>
                        <td style={{ fontSize: 12.5 }}>{shortName(doc)}</td>
                        <td><Tag type={STATUS_TAG[a.status]}>{STATUS_LABEL[a.status]}</Tag></td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            )
          }
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
          <div className="card">
            <div className="card-head"><h3>{monthTitle}</h3></div>
            <div className="card-body">
              <div className="mini-cal">
                {['Пн','Вт','Ср','Чт','Пт','Сб','Вс'].map(d => <div key={d} className="dow">{d}</div>)}
                {calDays.map((d, i) => (
                  <div key={i} className={`d${d.mute?' mute':''}${d.today?' today':''}${d.has?' has':''}`}>{d.n}</div>
                ))}
              </div>
            </div>
          </div>

          <div className="card">
            <div className="card-head"><h3>Быстрые действия</h3></div>
            <div className="card-body" style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
              {[
                { label: '+ Новая запись',    fn: onNewAppt },
                { label: '+ Добавить пациента', fn: () => onNavigate('patients') },
                { label: '+ Добавить врача',    fn: () => onNavigate('doctors') },
                { label: '+ Добавить кабинет',  fn: () => onNavigate('rooms') },
              ].map(q => (
                <button key={q.label} className="btn ghost" style={{ justifyContent: 'flex-start' }} onClick={q.fn}>{q.label}</button>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

// ── CALENDAR VIEW (by doctor columns, с навигацией по дням) ───────────────────
const MONTH_GEN = ['января','февраля','марта','апреля','мая','июня','июля','августа','сентября','октября','ноября','декабря'];
const DOW_SHORT = ['Вс','Пн','Вт','Ср','Чт','Пт','Сб'];
function addDays(isoDate, delta) {
  const [y, m, d] = isoDate.split('-').map(Number);
  const dt = new Date(Date.UTC(y, m - 1, d));
  dt.setUTCDate(dt.getUTCDate() + delta);
  return dt.toISOString().slice(0, 10);
}
function dowApp(isoDate) {
  // JS: Sun=0..Sat=6 → наша схема Пн=1..Вс=7
  const j = new Date(isoDate + 'T00:00:00').getDay();
  return j === 0 ? 7 : j;
}
function fmtDateLong(isoDate) {
  const d = new Date(isoDate + 'T00:00:00');
  return `${DOW_SHORT[d.getDay()]}, ${d.getDate()} ${MONTH_GEN[d.getMonth()]} ${d.getFullYear()}`;
}

// Расписание: 2 минуты = 1px по вертикали. Шкала от 00:00 до 24:00 = 2880px.
const PXM = 2;
const DAY_H = 24 * 60 * PXM;
const TIME_W = 60;     // ширина колонки часов
const COL_W  = 220;    // фиксированная ширина колонки врача

function timeToMin(t) {
  if (!t) return 0;
  const [h, m] = t.split(':').map(Number);
  return (h || 0) * 60 + (m || 0);
}

function CalendarView({ clinicId, doctors, patients, rooms, specs, appointments, schedules, today, onNewAppt, onApptClick }) {
  const [selectedDate, setSelectedDate] = React.useState(today);
  const [expandedAppt, setExpandedAppt] = React.useState(null);
  const scrollRef = React.useRef(null);

  const dayAppts    = appointments.filter(function(a) { return a.scheduledAt && a.scheduledAt.startsWith(selectedDate); });
  const patientMap  = Object.fromEntries(patients.map(function(p) { return [p.id, p]; }));
  const roomMap     = Object.fromEntries(rooms.map(function(r) { return [r.id, r]; }));
  const specMap     = Object.fromEntries(specs.map(function(s) { return [s.id, s]; }));

  const clinicRoomIds    = new Set(rooms.map(function(r) { return r.id; }));
  const clinicSchedules  = schedules.filter(function(s) { return clinicRoomIds.has(s.roomId); });
  const dow              = dowApp(selectedDate);
  const daySchedules     = clinicSchedules.filter(function(s) { return s.dayOfWeek === dow; });
  const docSchedMap      = Object.fromEntries(daySchedules.map(function(s) { return [s.doctorId, s]; }));
  const activeDoctorIds  = Array.from(new Set(daySchedules.map(function(s) { return s.doctorId; })));
  const displayDoctors   = doctors.filter(function(d) { return activeDoctorIds.indexOf(d.id) !== -1; });

  function handleWorkClick(doc, sched, e) {
    e.stopPropagation();
    var rect       = e.currentTarget.getBoundingClientRect();
    var yOffset    = e.clientY - rect.top;
    var fromMin    = timeToMin(sched.timeFrom);
    var toMin      = timeToMin(sched.timeTo);
    var slotDur    = sched.slotDurationMin || 30;
    var clickedMin = fromMin + Math.floor(yOffset / PXM);
    var roundedMin = Math.floor(clickedMin / slotDur) * slotDur;
    var clampedMin = Math.min(Math.max(roundedMin, fromMin), toMin - slotDur);
    var hh = String(Math.floor(clampedMin / 60)).padStart(2, '0');
    var mm = String(clampedMin % 60).padStart(2, '0');
    onNewAppt(null, { specId: doc.specialization_id, date: selectedDate, doctorId: doc.id, preSlotTime: hh + ':' + mm });
  }

  React.useLayoutEffect(function() {
    if (!scrollRef.current || displayDoctors.length === 0) return;
    var firstMin = null;
    for (var i = 0; i < displayDoctors.length; i++) {
      var s = docSchedMap[displayDoctors[i].id];
      if (!s) continue;
      var m = timeToMin(s.timeFrom);
      if (firstMin == null || m < firstMin) firstMin = m;
    }
    if (firstMin != null) scrollRef.current.scrollTop = Math.max(0, firstMin * PXM - 40);
  }, [selectedDate, displayDoctors.length]);

  var navBtn = { padding: '4px 10px', fontSize: 13, display: 'inline-flex', alignItems: 'center', justifyContent: 'center', minWidth: 30 };

  // 30-min slots: 48 per day
  var halfSlots = [];
  for (var si = 0; si < 48; si++) {
    halfSlots.push(si * 30);
  }

  return (
    <div className="cal-page">
      <div className="cal-nav">
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <button className="btn ghost" style={navBtn} onClick={() => setSelectedDate(function(d) { return addDays(d, -1); })}>←</button>
          <button className="btn ghost" style={Object.assign({}, navBtn, { fontSize: 11 })} onClick={() => setSelectedDate(today)}>Сегодня</button>
          <button className="btn ghost" style={navBtn} onClick={() => setSelectedDate(function(d) { return addDays(d, 1); })}>→</button>
        </div>
        <div>
          <span style={{ fontWeight: 600, fontSize: 15 }}>Расписание</span>
          <span style={{ marginLeft: 10, color: 'var(--ink-3)', fontSize: 13 }}>{fmtDateLong(selectedDate)}</span>
        </div>
        <div style={{ marginLeft: 'auto', display: 'flex', gap: 8, alignItems: 'center' }}>
          <input type="date" className="input-el" style={{ padding: '5px 8px', fontSize: 12, width: 140 }}
            value={selectedDate} onChange={function(e) { if (e.target.value) setSelectedDate(e.target.value); }} />
          <div style={{ display: 'flex', gap: 14, alignItems: 'center', fontSize: 12, color: 'var(--ink-3)' }}>
            <span><span className="tag ok dot" style={{ marginRight: 3 }}></span>Запланирована</span>
            <span><span className="tag info dot" style={{ marginRight: 3 }}></span>Завершена</span>
            <span><span className="tag err dot" style={{ marginRight: 3 }}></span>Отменена</span>
          </div>
          <button className="btn primary" onClick={onNewAppt}>+ Запись</button>
        </div>
      </div>

      <div className="cal-scroll" ref={scrollRef}>
        {displayDoctors.length === 0
          ? <Empty text={"Нет врачей с расписанием на " + fmtDateLong(selectedDate)} />
          : (
            <div className="cal-rows">
              <div className="cal-row-head">
                <div style={{ width: TIME_W, flexShrink: 0, background: 'var(--bg-soft)', borderRight: '1px solid var(--line)' }}></div>
                {displayDoctors.map(function(doc) {
                  var sched    = docSchedMap[doc.id];
                  var room     = sched ? roomMap[sched.roomId] : null;
                  var specName = specMap[doc.specialization_id] ? specMap[doc.specialization_id].name : '—';
                  return (
                    <div key={doc.id} className="doc-head" style={{ width: COL_W }}>
                      <Av p={doc} size={24} fs={9} />
                      <div style={{ minWidth: 0, flex: 1 }}>
                        <div style={{ fontSize: 12.5, fontWeight: 500, whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>{shortName(doc)}</div>
                        <div className="specialty">{specName}</div>
                        <div className="mono" style={{ fontSize: 10.5, color: 'var(--ink-3)' }}>
                          {sched ? sched.timeFrom + '–' + sched.timeTo : '—'}
                        </div>
                      </div>
                      {room && <div className="doc-room">{"№ " + room.number}</div>}
                    </div>
                  );
                })}
              </div>

              <div className="cal-row-body">
                <div className="time-col" style={{ width: TIME_W, height: DAY_H }}>
                  {halfSlots.map(function(totalMin) {
                    var hh = String(Math.floor(totalMin / 60)).padStart(2, '0');
                    var isHalf = (totalMin % 60) === 30;
                    return (
                      <div key={totalMin}
                        className={isHalf ? 'time-label time-label-half' : 'time-label'}
                        style={{ top: totalMin * PXM }}>
                        {isHalf ? hh + ':30' : hh + ':00'}
                      </div>
                    );
                  })}
                </div>

                {displayDoctors.map(function(doc) {
                  var sched   = docSchedMap[doc.id];
                  var fromMin = sched ? timeToMin(sched.timeFrom) : 0;
                  var toMin   = sched ? timeToMin(sched.timeTo)   : 0;
                  var myAppts = dayAppts.filter(function(a) { return a.doctorId === doc.id; });
                  var sorted  = myAppts.slice().sort(function(a, b) { return (a.status === 2 ? 0 : 1) - (b.status === 2 ? 0 : 1); });

                  return (
                    <div key={doc.id} className="doc-col" style={{ width: COL_W, height: DAY_H }}>
                      {sched && (
                        <div className="work-window"
                          style={{ top: fromMin * PXM, height: (toMin - fromMin) * PXM, zIndex: 1 }}
                          onClick={function(e) { handleWorkClick(doc, sched, e); }}
                          title="Нажмите — создать запись на это время"
                        />
                      )}
                      {halfSlots.map(function(totalMin) {
                        if (totalMin === 0) return null;
                        var isHalf = (totalMin % 60) === 30;
                        return (
                          <div key={totalMin}
                            className={isHalf ? 'half-line' : 'hour-line'}
                            style={{ top: totalMin * PXM, zIndex: 2 }}
                          />
                        );
                      })}
                      {sorted.map(function(a) {
                        var startMin  = timeToMin(a.scheduledAt.slice(11, 16));
                        var dur       = a.durationMin || (sched && sched.slotDurationMin) || 30;
                        var endMin    = startMin + dur;
                        var endStr    = String(Math.floor(endMin / 60)).padStart(2, '0') + ':' + String(endMin % 60).padStart(2, '0');
                        var pat       = patientMap[a.patientId];
                        var tag       = STATUS_TAG[a.status];
                        var cls       = 'appt' + (tag !== 'ok' ? ' ' + tag : '') + (a.status === 2 ? ' cancelled' : '');
                        var naturalH  = Math.max(22, dur * PXM - 2);
                        var isExp     = expandedAppt === a.id;
                        return (
                          <div key={a.id}
                            className={cls}
                            style={{
                              top:        startMin * PXM + 1,
                              minHeight:  naturalH,
                              maxHeight:  isExp ? 300 : naturalH,
                              height:     'auto',
                              overflow:   'hidden',
                              transition: 'max-height 0.22s ease, box-shadow 0.15s',
                              zIndex:     isExp ? 10 : (a.status === 2 ? 1 : 3),
                              boxShadow:  isExp ? '0 4px 16px rgba(0,0,0,0.18)' : 'none',
                            }}
                            onMouseEnter={function() { setExpandedAppt(a.id); }}
                            onMouseLeave={function() { setExpandedAppt(null); }}
                            onClick={function(e) { e.stopPropagation(); onApptClick(a); }}
                          >
                            <div className="pname">{shortName(pat)}</div>
                            <div className="ptime">{a.scheduledAt ? a.scheduledAt.slice(11,16) : ''}&ndash;{endStr}</div>
                            <div className="ptime" style={{ marginTop: 2, opacity: 0.85 }}>{STATUS_LABEL[a.status]}</div>
                            <div className="ptime" style={{ marginTop: 3, opacity: 0.7, whiteSpace: 'normal', lineHeight: 1.3 }}>{fullName(pat)}</div>
                          </div>
                        );
                      })}
                    </div>
                  );
                })}
              </div>
            </div>
          )
        }
      </div>
    </div>
  );
}

// ── PATIENTS VIEW ─────────────────────────────────────────────────────────────
function PatientsView({ clinicId, patients, appointments, specs, today, onAddPatient, onDeletePatient, onNewAppt, onChangeApptStatus, toast }) {
  const [search, setSearch] = React.useState('');
  const [selected, setSelected] = React.useState(null);
  const [tab, setTab] = React.useState('info');
  const [confirmDel, setConfirmDel] = React.useState(false);
  const [confirmCancel, setConfirmCancel] = React.useState(null);

  const filtered = patients.filter(p =>
    !search || fullName(p).toLowerCase().includes(search.toLowerCase()) ||
    p.insuranceNum?.includes(search) || p.phone?.includes(search)
  );

  const patAppts = selected
    ? appointments.filter(a => a.patientId === selected.id).sort((a, b) => b.scheduledAt?.localeCompare(a.scheduledAt))
    : [];

  function doDelete() {
    setConfirmDel(false);
    onDeletePatient(selected.id);
    setSelected(null);
  }

  return (
    <div className="main">
      <div className="page-head">
        <div><h1>Пациенты</h1><div className="page-sub">Всего — {patients.length}</div></div>
        <div className="head-actions">
          <button className="btn primary" onClick={onAddPatient}>+ Добавить пациента</button>
        </div>
      </div>

      <div className="detail">
        {/* List */}
        <div className="card" style={{ padding: 0 }}>
          <div className="card-head">
            <div className="search" style={{ flex: 1, maxWidth: 'none', margin: 0 }}>
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"><circle cx="11" cy="11" r="8"/><path d="M21 21l-4.3-4.3"/></svg>
              <input style={{ border: 'none', background: 'transparent', font: 'inherit', fontSize: 13, outline: 'none', flex: 1, color: 'var(--ink)' }}
                placeholder="Поиск по ФИО, полису, телефону…"
                value={search} onChange={e => setSearch(e.target.value)} />
            </div>
          </div>
          {filtered.length === 0
            ? <Empty text="Пациентов не найдено" />
            : (
              <table className="list">
                <thead><tr><th>Пациент</th><th>Пол</th><th>Полис</th><th>Телефон</th></tr></thead>
                <tbody>
                  {filtered.map(p => (
                    <tr key={p.id} style={selected?.id===p.id?{background:'var(--accent-tint)'}:{}} onClick={() => { setSelected(p); setTab('info'); }}>
                      <td className="cell-name">
                        <Av p={p} size={28} />
                        <div>
                          <div style={selected?.id===p.id?{fontWeight:600}:{}}>{fullName(p)}</div>
                          <div className="mono" style={{ fontSize: 11, color: 'var(--ink-3)' }}>{fmtDate(p.birthDate)}</div>
                        </div>
                      </td>
                      <td style={{ fontSize: 12, color: 'var(--ink-3)' }}>{GENDER_LABEL[p.gender ?? 2]}</td>
                      <td className="mono" style={{ fontSize: 12 }}>{p.insuranceNum || '—'}</td>
                      <td className="mono" style={{ fontSize: 12 }}>{p.phone || '—'}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )
          }
        </div>

        {/* Detail panel */}
        {selected ? (
          <div className="card" style={{ padding: 0, alignSelf: 'start' }}>
            <div style={{ padding: '14px 16px', borderBottom: '1px solid var(--line-soft)', display: 'flex', gap: 10, alignItems: 'center' }}>
              <Av p={selected} size={36} fs={13} />
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ fontWeight: 600, fontSize: 14 }}>{fullName(selected)}</div>
                <div style={{ fontSize: 11, color: 'var(--ink-3)' }}>ID {selected.id} · {ageOf(selected.birthDate, today)}</div>
              </div>
              <button className="btn ghost" style={{ padding: '4px 8px', fontSize: 12, color: 'var(--danger)' }} onClick={() => setConfirmDel(true)}>Удалить</button>
            </div>
            <div className="detail-tabs">
              {['info','history'].map(t => (
                <div key={t} className={`detail-tab${tab===t?' on':''}`} onClick={() => setTab(t)}>
                  {t === 'info' ? 'Информация' : `История (${patAppts.length})`}
                </div>
              ))}
              <div style={{ marginLeft: 'auto' }}>
                <button className="btn primary" style={{ padding: '4px 10px', fontSize: 12 }} onClick={() => onNewAppt(selected)}>+ Запись</button>
              </div>
            </div>
            <div className="card-body">
              {tab === 'info' ? (
                <div>
                  {[
                    ['Дата рождения', fmtDate(selected.birthDate)],
                    ['Пол', GENDER_LABEL[selected.gender ?? 2]],
                    ['Полис ОМС', selected.insuranceNum || '—'],
                    ['Телефон', selected.phone || '—'],
                  ].map(([l, v]) => (
                    <div key={l} style={{ display: 'flex', justifyContent: 'space-between', padding: '7px 0', borderBottom: '1px solid var(--line-soft)', fontSize: 13 }}>
                      <span style={{ color: 'var(--ink-3)' }}>{l}</span>
                      <span className="mono">{v}</span>
                    </div>
                  ))}
                </div>
              ) : (
                patAppts.length === 0
                  ? <Empty text="История посещений пуста" />
                  : (
                    <div className="timeline">
                      {patAppts.map(a => (
                        <div key={a.id} className="tl-item">
                          <div style={{ display:'flex', alignItems:'center', gap:8 }}>
                            <div className="tl-date" style={{ flex:1 }}>{a.scheduledAt?.slice(0, 10)}</div>
                            {a.status === 0 && onChangeApptStatus && (
                              <button className="btn ghost" style={{ padding:'2px 8px', fontSize:11, color:'var(--danger)', borderColor:'var(--danger-soft)' }}
                                onClick={() => setConfirmCancel(a)}>Отменить</button>
                            )}
                          </div>
                          <div className="tl-title">{a.scheduledAt?.slice(11)} — <Tag type={STATUS_TAG[a.status]}>{STATUS_LABEL[a.status]}</Tag></div>
                          {a.notes && <div className="tl-note">{a.notes}</div>}
                        </div>
                      ))}
                    </div>
                  )
              )}
            </div>
          </div>
        ) : (
          <div className="card" style={{ display: 'grid', placeItems: 'center', minHeight: 200 }}>
            <Empty text="Выберите пациента из списка" />
          </div>
        )}
      </div>

      {confirmDel && (
        <ConfirmModal
          text={`Удалить пациента «${fullName(selected)}»? Все связанные записи будут удалены.`}
          onConfirm={doDelete}
          onClose={() => setConfirmDel(false)}
        />
      )}
      {confirmCancel && (
        <Modal title="Отменить запись" onClose={() => setConfirmCancel(null)} width={380}>
          <p style={{ color: 'var(--ink-2)', fontSize: 14, marginBottom: 20 }}>
            Отменить запись на {confirmCancel.scheduledAt}?
          </p>
          <div style={{ display: 'flex', gap: 8, justifyContent: 'flex-end' }}>
            <button className="btn ghost" onClick={() => setConfirmCancel(null)}>Назад</button>
            <button className="btn danger" onClick={() => { onChangeApptStatus(confirmCancel.id, 2); setConfirmCancel(null); }}>Отменить запись</button>
          </div>
        </Modal>
      )}
    </div>
  );
}

// ── DOCTORS VIEW ──────────────────────────────────────────────────────────────
function DoctorsView({ clinicId, doctors, schedules, rooms, specs, appointments, today, onAddDoctor, onDeleteDoctor, onAddSchedule, onDeleteSchedule, toast }) {
  const [search, setSearch] = React.useState('');
  const [selected, setSelected] = React.useState(null);
  const [tab, setTab] = React.useState('info');
  const [confirmDel, setConfirmDel] = React.useState(false);
  const [confirmDelSched, setConfirmDelSched] = React.useState(null);

  const specMap = Object.fromEntries(specs.map(s => [s.id, s]));
  const roomMap = Object.fromEntries(rooms.map(r => [r.id, r]));

  const filtered = doctors.filter(d =>
    !search || fullName(d).toLowerCase().includes(search.toLowerCase()) ||
    specMap[d.specialization_id]?.name?.toLowerCase().includes(search.toLowerCase())
  );

  const docScheds = selected ? schedules.filter(s => s.doctorId === selected.id) : [];
  const docAppts = selected ? appointments.filter(a => a.doctorId === selected.id) : [];
  const upcoming = docAppts.filter(a => a.scheduledAt >= today && a.status !== 2)
    .sort((a, b) => a.scheduledAt.localeCompare(b.scheduledAt));

  return (
    <div className="main">
      <div className="page-head">
        <div><h1>Врачи</h1><div className="page-sub">Всего — {doctors.length}</div></div>
        <div className="head-actions">
          <button className="btn primary" onClick={onAddDoctor}>+ Добавить врача</button>
        </div>
      </div>

      <div className="detail">
        <div className="card" style={{ padding: 0 }}>
          <div className="card-head">
            <div className="search" style={{ flex: 1, maxWidth: 'none', margin: 0 }}>
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"><circle cx="11" cy="11" r="8"/><path d="M21 21l-4.3-4.3"/></svg>
              <input style={{ border: 'none', background: 'transparent', font: 'inherit', fontSize: 13, outline: 'none', flex: 1, color: 'var(--ink)' }}
                placeholder="Поиск по имени или специальности…"
                value={search} onChange={e => setSearch(e.target.value)} />
            </div>
          </div>
          {filtered.length === 0 ? <Empty text="Врачей не найдено" /> : (
            <table className="list">
              <thead><tr><th>Врач</th><th>Специализация</th><th>Телефон</th><th>Дней в неделю</th></tr></thead>
              <tbody>
                {filtered.map(d => {
                  const ds = schedules.filter(s => s.doctorId === d.id);
                  return (
                    <tr key={d.id} style={selected?.id===d.id?{background:'var(--accent-tint)'}:{}} onClick={() => { setSelected(d); setTab('info'); }}>
                      <td className="cell-name"><Av p={d} size={28} /><div><div style={selected?.id===d.id?{fontWeight:600}:{}}>{fullName(d)}</div><div style={{ fontSize:11, color:'var(--ink-3)' }} className="mono">ID {d.id}</div></div></td>
                      <td style={{ fontSize: 12.5 }}>{specMap[d.specialization_id]?.name || '—'}</td>
                      <td className="mono" style={{ fontSize: 12 }}>{d.phone || '—'}</td>
                      <td><span className="tag">{ds.length} д.</span></td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          )}
        </div>

        {selected ? (
          <div className="card" style={{ padding: 0, alignSelf: 'start' }}>
            <div style={{ padding: '14px 16px', borderBottom: '1px solid var(--line-soft)', display: 'flex', gap: 10, alignItems: 'center' }}>
              <Av p={selected} size={36} fs={13} />
              <div style={{ flex: 1 }}>
                <div style={{ fontWeight: 600, fontSize: 14 }}>{fullName(selected)}</div>
                <div style={{ fontSize: 11, color: 'var(--ink-3)' }}>{specMap[selected.specialization_id]?.name || '—'}</div>
              </div>
              <button className="btn ghost" style={{ padding: '4px 8px', fontSize: 12, color: 'var(--danger)' }} onClick={() => setConfirmDel(true)}>Удалить</button>
            </div>
            <div className="detail-tabs">
              {[['info','Информация'],['schedule',`Расписание (${docScheds.length})`],['appts',`Записи (${upcoming.length})`]].map(([t,l]) => (
                <div key={t} className={`detail-tab${tab===t?' on':''}`} onClick={() => setTab(t)}>{l}</div>
              ))}
            </div>
            <div className="card-body">
              {tab === 'info' && (
                <div>
                  {[['Телефон', selected.phone||'—'],['Специализация', specMap[selected.specialization_id]?.name||'—']].map(([l,v]) => (
                    <div key={l} style={{ display:'flex', justifyContent:'space-between', padding:'7px 0', borderBottom:'1px solid var(--line-soft)', fontSize:13 }}>
                      <span style={{ color:'var(--ink-3)' }}>{l}</span><span className="mono">{v}</span>
                    </div>
                  ))}
                </div>
              )}
              {tab === 'schedule' && (
                <div>
                  <button className="btn ghost" style={{ width:'100%', marginBottom:10, justifyContent:'center' }} onClick={() => onAddSchedule(selected)}>+ Добавить рабочий день</button>
                  {docScheds.length === 0 ? <Empty text="Расписание не задано" /> : (
                    <div style={{ display:'flex', flexDirection:'column', gap:6 }}>
                      {docScheds.sort((a,b)=>a.dayOfWeek-b.dayOfWeek).map(s => {
                        const room = roomMap[s.roomId];
                        return (
                          <div key={s.id} style={{ display:'flex', alignItems:'center', gap:8, padding:'8px 10px', background:'var(--bg-soft)', borderRadius:6, fontSize:13 }}>
                            <span style={{ fontWeight:500, minWidth:24 }}>{DAYS[s.dayOfWeek]}</span>
                            <span className="mono" style={{ fontSize:12 }}>{s.timeFrom}–{s.timeTo}</span>
                            <span style={{ color:'var(--ink-3)', fontSize:12 }}>каб. {room?.number||s.roomId}</span>
                            <span style={{ color:'var(--ink-3)', fontSize:11, marginLeft:'auto' }}>{s.slotDurationMin} мин</span>
                            <button className="icon-btn" style={{ color:'var(--danger)' }} onClick={() => setConfirmDelSched(s.id)}>
                              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round"><path d="M18 6L6 18M6 6l12 12"/></svg>
                            </button>
                          </div>
                        );
                      })}
                    </div>
                  )}
                </div>
              )}
              {tab === 'appts' && (
                upcoming.length === 0 ? <Empty text="Предстоящих записей нет" /> : (
                  <div style={{ display:'flex', flexDirection:'column', gap:6 }}>
                    {upcoming.slice(0,10).map(a => (
                      <div key={a.id} style={{ display:'flex', gap:8, alignItems:'center', padding:'7px 10px', background:'var(--bg-soft)', borderRadius:6, fontSize:13 }}>
                        <span className="mono" style={{ fontSize:12, color:'var(--ink-3)' }}>{a.scheduledAt}</span>
                        <Tag type={STATUS_TAG[a.status]}>{STATUS_LABEL[a.status]}</Tag>
                      </div>
                    ))}
                  </div>
                )
              )}
            </div>
          </div>
        ) : (
          <div className="card" style={{ display:'grid', placeItems:'center', minHeight:200 }}>
            <Empty text="Выберите врача из списка" />
          </div>
        )}
      </div>

      {confirmDel && <ConfirmModal text={`Удалить врача «${fullName(selected)}»? Его расписания и запланированные записи будут удалены.`} onConfirm={() => { setConfirmDel(false); onDeleteDoctor(selected.id); setSelected(null); }} onClose={() => setConfirmDel(false)} />}
      {confirmDelSched && <ConfirmModal text="Удалить этот рабочий день из расписания?" onConfirm={() => { onDeleteSchedule(confirmDelSched); setConfirmDelSched(null); }} onClose={() => setConfirmDelSched(null)} />}
    </div>
  );
}

// ── ROOMS VIEW ────────────────────────────────────────────────────────────────
function RoomsView({ clinicId, rooms, schedules, doctors, specs, onAddRoom, onDeleteRoom }) {
  const [selected, setSelected] = React.useState(null);
  const [confirmDel, setConfirmDel] = React.useState(false);

  const doctorMap = Object.fromEntries(doctors.map(d => [d.id, d]));
  const specMap = Object.fromEntries(specs.map(s => [s.id, s]));

  const roomScheds = selected ? schedules.filter(s => s.roomId === selected.id) : [];

  return (
    <div className="main">
      <div className="page-head">
        <div><h1>Кабинеты</h1><div className="page-sub">{rooms.length} кабинетов в поликлинике</div></div>
        <div className="head-actions">
          <button className="btn primary" onClick={onAddRoom}>+ Добавить кабинет</button>
        </div>
      </div>

      <div className="detail">
        <div className="card" style={{ padding: 0 }}>
          <div className="card-head"><h3>Список кабинетов</h3></div>
          {rooms.length === 0 ? <Empty text="Кабинетов нет" /> : (
            <table className="list">
              <thead><tr><th>Номер</th><th>Этаж</th><th>Расписаний</th></tr></thead>
              <tbody>
                {rooms.sort((a,b)=>a.floor-b.floor||a.number?.localeCompare(b.number)).map(r => {
                  const rs = schedules.filter(s => s.roomId === r.id);
                  return (
                    <tr key={r.id} style={selected?.id===r.id?{background:'var(--accent-tint)'}:{}} onClick={() => setSelected(r)}>
                      <td style={{ fontWeight:500 }}>Кабинет № {r.number}</td>
                      <td><span className="tag">{r.floor} этаж</span></td>
                      <td><span className="tag">{rs.length}</span></td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          )}
        </div>

        {selected ? (
          <div className="card" style={{ padding: 0, alignSelf: 'start' }}>
            <div style={{ padding:'14px 16px', borderBottom:'1px solid var(--line-soft)', display:'flex', gap:10, alignItems:'center' }}>
              <div style={{ width:36, height:36, borderRadius:8, background:'var(--accent-tint)', display:'grid', placeItems:'center', fontFamily:'monospace', fontSize:13, fontWeight:600, color:'var(--accent)' }}>
                {selected.number}
              </div>
              <div style={{ flex:1 }}>
                <div style={{ fontWeight:600, fontSize:14 }}>Кабинет № {selected.number}</div>
                <div style={{ fontSize:11, color:'var(--ink-3)' }}>{selected.floor} этаж · ID {selected.id}</div>
              </div>
              <button className="btn ghost" style={{ padding:'4px 8px', fontSize:12, color:'var(--danger)' }} onClick={() => setConfirmDel(true)}>Удалить</button>
            </div>
            <div className="card-body">
              <div style={{ fontWeight:500, fontSize:12, color:'var(--ink-3)', textTransform:'uppercase', letterSpacing:'0.06em', marginBottom:10 }}>Расписание кабинета</div>
              {roomScheds.length === 0 ? <Empty text="Расписания не назначены" /> : (
                <div style={{ display:'flex', flexDirection:'column', gap:6 }}>
                  {roomScheds.sort((a,b)=>a.dayOfWeek-b.dayOfWeek).map(s => {
                    const doc = doctorMap[s.doctorId];
                    return (
                      <div key={s.id} style={{ padding:'10px 12px', background:'var(--bg-soft)', borderRadius:6, fontSize:13 }}>
                        <div style={{ display:'flex', gap:8, alignItems:'center', marginBottom:3 }}>
                          <span style={{ fontWeight:500 }}>{DAYS[s.dayOfWeek]}</span>
                          <span className="mono" style={{ fontSize:12 }}>{s.timeFrom}–{s.timeTo}</span>
                        </div>
                        <div style={{ color:'var(--ink-3)', fontSize:12 }}>{shortName(doc)} · {specMap[doc?.specialization_id]?.name || '—'}</div>
                      </div>
                    );
                  })}
                </div>
              )}
            </div>
          </div>
        ) : (
          <div className="card" style={{ display:'grid', placeItems:'center', minHeight:200 }}>
            <Empty text="Выберите кабинет" />
          </div>
        )}
      </div>

      {confirmDel && <ConfirmModal text={`Удалить кабинет № ${selected.number}? Связанные расписания будут удалены.`} onConfirm={() => { setConfirmDel(false); onDeleteRoom(selected.id); setSelected(null); }} onClose={() => setConfirmDel(false)} />}
    </div>
  );
}

// ── NEW APPOINTMENT WIZARD ────────────────────────────────────────────────────
function NewAppointmentView({ clinicId, patients, doctors, specs, schedules, appointments, onCreated, toast, prefillPatient, prefillAppt }) {
  const pa = prefillAppt || {};
  const hasSpec = pa.specId != null && pa.specId !== '';
  const hasDate = !!pa.date;
  const hasDoc = pa.doctorId != null && pa.doctorId !== '';
  const hasSlot = !!pa.slotTime;
  const initialStep =
    hasSpec && hasDate && hasDoc && hasSlot ? 5 :
    hasSpec && hasDate && hasDoc ? 4 :
    hasSpec && hasDate ? 3 :
    hasSpec ? 2 : 1;
  const [step, setStep] = React.useState(initialStep);
  const [specId, setSpecId] = React.useState(hasSpec ? String(pa.specId) : '');
  const [date, setDate] = React.useState(pa.date || '');
  const [doctorId, setDoctorId] = React.useState(hasDoc ? String(pa.doctorId) : '');
  const [slots, setSlots] = React.useState([]);
  // preSlotTime предвыбирает слот на шаге 4, slotTime используется для перехода на шаг 5
  const [slotTime, setSlotTime] = React.useState(pa.slotTime || pa.preSlotTime || '');
  const [patientId, setPatientId] = React.useState(prefillPatient?.id ?? '');
  const [loading, setLoading] = React.useState(false);
  const [availDates, setAvailDates] = React.useState([]);
  const [availDoctors, setAvailDoctors] = React.useState([]);

  const specDoctors = doctors.filter(d => !specId || d.specialization_id === parseInt(specId));
  const specMap = Object.fromEntries(specs.map(s => [s.id, s]));
  const patientMap = Object.fromEntries(patients.map(p => [p.id, p]));

  async function loadDates() {
    if (!specId) return;
    setLoading(true);
    try {
      const d = await apiGet(`/schedules/available-days?specId=${specId}&clinicId=${clinicId}&daysAhead=14`);
      setAvailDates(Array.isArray(d) ? d : []);
    } catch(e) {
      setAvailDates([]);
    }
    setLoading(false);
  }

  async function loadDoctors() {
    if (!specId || !date) return;
    setLoading(true);
    try {
      const ids = await apiGet(`/schedules/doctors-by-day?specId=${specId}&date=${date}&clinicId=${clinicId}`);
      setAvailDoctors(Array.isArray(ids) ? ids : []);
    } catch(e) {
      setAvailDoctors(specDoctors.map(d=>d.id));
    }
    setLoading(false);
  }

  async function loadSlots() {
    if (!doctorId || !date) return;
    setLoading(true);
    try {
      const s = await apiGet(`/schedules/time-slots?doctorId=${doctorId}&date=${date}&clinicId=${clinicId}`);
      setSlots(Array.isArray(s) ? s : []);
    } catch(e) {
      setSlots([]);
    }
    setLoading(false);
  }

  React.useEffect(() => { if (step === 2) loadDates(); }, [step]);
  React.useEffect(() => { if (step === 3) loadDoctors(); }, [step]);
  React.useEffect(() => { if (step === 4) loadSlots(); }, [step]);

  async function confirm() {
    if (!patientId || !doctorId || !date || !slotTime) { toast('Заполните все поля', false); return; }
    setLoading(true);
    try {
      await apiPost('/appointments', { patientId: parseInt(patientId), doctorId: parseInt(doctorId), scheduledAt: `${date} ${slotTime}` });
      toast('Запись создана', true);
      onCreated();
    } catch(e) {
      toast(e.message, false);
    }
    setLoading(false);
  }

  const stepLabels = ['Специализация','Дата','Врач','Слот','Подтверждение'];

  return (
    <div className="main">
      <div className="page-head">
        <div><h1>Новая запись</h1><div className="page-sub">Пошаговое создание приёма</div></div>
      </div>

      <div className="steps">
        {stepLabels.map((l, i) => {
          const n = i + 1;
          const cls = n < step ? 'done' : n === step ? 'cur' : '';
          return (
            <React.Fragment key={l}>
              {i > 0 && <div className={`line${n <= step ? ' on' : ''}`}></div>}
              <div className={`step ${cls}`}>
                <div className="n">{n < step ? '✓' : n}</div>
                {l}
              </div>
            </React.Fragment>
          );
        })}
      </div>

      <div className="card" style={{ maxWidth: 560 }}>
        <div className="card-body">
          {step === 1 && (
            <div>
              <Field label="Специализация">
                <Select value={specId} onChange={e => setSpecId(e.target.value)}>
                  <option value="">— Выберите специализацию —</option>
                  {specs.map(s => <option key={s.id} value={s.id}>{s.name}</option>)}
                </Select>
              </Field>
              <button className="btn primary" disabled={!specId} onClick={() => setStep(2)}>Далее →</button>
            </div>
          )}
          {step === 2 && (
            <div>
              <Field label="Дата приёма" hint="Доступные даты загружены с сервера">
                {loading ? <Spinner /> : (
                  availDates.length > 0
                    ? <div style={{ display:'flex', flexWrap:'wrap', gap:6 }}>
                        {availDates.map(d => (
                          <div key={d} className={`chip${date===d?' on':''}`} onClick={() => setDate(d)}>{fmtDate(d)}</div>
                        ))}
                      </div>
                    : <div>
                        <Input type="date" value={date} onChange={e => setDate(e.target.value)} />
                        <div className="hint" style={{ marginTop:4 }}>Нет доступных дат с сервера — введите вручную</div>
                      </div>
                )}
              </Field>
              <div style={{ display:'flex', gap:8 }}>
                <button className="btn ghost" onClick={() => setStep(1)}>← Назад</button>
                <button className="btn primary" disabled={!date} onClick={() => setStep(3)}>Далее →</button>
              </div>
            </div>
          )}
          {step === 3 && (
            <div>
              <Field label="Врач">
                {loading ? <Spinner /> : (
                  <Select value={doctorId} onChange={e => setDoctorId(e.target.value)}>
                    <option value="">— Выберите врача —</option>
                    {doctors.filter(d => !specId || d.specialization_id === parseInt(specId))
                      .filter(d => availDoctors.length === 0 || availDoctors.includes(d.id))
                      .map(d => <option key={d.id} value={d.id}>{fullName(d)}</option>)
                    }
                  </Select>
                )}
              </Field>
              <div style={{ display:'flex', gap:8 }}>
                <button className="btn ghost" onClick={() => setStep(2)}>← Назад</button>
                <button className="btn primary" disabled={!doctorId} onClick={() => setStep(4)}>Далее →</button>
              </div>
            </div>
          )}
          {step === 4 && (
            <div>
              <Field label="Время приёма">
                {loading ? <Spinner /> : (
                  slots.length > 0
                    ? <div style={{ display:'flex', flexWrap:'wrap', gap:6 }}>
                        {slots.map(s => (
                          <div key={s.time} className={`chip${slotTime===s.time?' on':''}`} onClick={() => setSlotTime(s.time)}>
                            {s.time}
                          </div>
                        ))}
                      </div>
                    : <div>
                        <Input type="time" value={slotTime} onChange={e => setSlotTime(e.target.value)} />
                        <div className="hint" style={{ marginTop:4 }}>Свободных слотов не найдено — введите время вручную</div>
                      </div>
                )}
              </Field>
              <div style={{ display:'flex', gap:8 }}>
                <button className="btn ghost" onClick={() => setStep(3)}>← Назад</button>
                <button className="btn primary" disabled={!slotTime} onClick={() => setStep(5)}>Далее →</button>
              </div>
            </div>
          )}
          {step === 5 && (
            <div>
              <Field label="Пациент">
                <Select value={patientId} onChange={e => setPatientId(e.target.value)}>
                  <option value="">— Выберите пациента —</option>
                  {patients.map(p => <option key={p.id} value={p.id}>{fullName(p)}</option>)}
                </Select>
              </Field>
              <div style={{ padding:'12px 14px', background:'var(--bg-soft)', borderRadius:8, marginBottom:14, fontSize:13 }}>
                <div style={{ color:'var(--ink-3)', fontSize:11, marginBottom:6, textTransform:'uppercase', letterSpacing:'0.06em' }}>Сводка записи</div>
                {[
                  ['Специализация', specs.find(s=>s.id===parseInt(specId))?.name||specId],
                  ['Дата и время', `${date} ${slotTime}`],
                  ['Врач', fullName(doctors.find(d=>d.id===parseInt(doctorId)))],
                  ['Пациент', patientId ? fullName(patients.find(p=>p.id===parseInt(patientId))) : '—'],
                ].map(([l,v]) => (
                  <div key={l} style={{ display:'flex', justifyContent:'space-between', padding:'4px 0', borderBottom:'1px solid var(--line-soft)' }}>
                    <span style={{ color:'var(--ink-3)' }}>{l}</span><span className="mono">{v}</span>
                  </div>
                ))}
              </div>
              <div style={{ display:'flex', gap:8 }}>
                <button className="btn ghost" onClick={() => setStep(4)}>← Назад</button>
                <button className="btn primary" disabled={loading || !patientId} onClick={confirm}>
                  {loading ? 'Создаём…' : 'Создать запись'}
                </button>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

// ── ADD MODALS ────────────────────────────────────────────────────────────────
function AddPatientModal({ clinicId, onSave, onClose, toast }) {
  const [f, setF] = React.useState({ insuranceNum:'', last_name:'', first_name:'', middle_name:'', gender:'2', birthDate:'', phone:'' });
  const set = k => e => setF(p => ({ ...p, [k]: e.target.value }));
  async function save() {
    try {
      await apiPost('/patients', { ...f, gender: parseInt(f.gender), clinic_id: clinicId });
      toast('Пациент добавлен', true);
      onSave();
    } catch(e) { toast(e.message, false); }
  }
  return (
    <Modal title="Добавить пациента" onClose={onClose} width={500}>
      <div className="row2">
        <Field label="Фамилия"><Input placeholder="Иванов" value={f.last_name} onChange={set('last_name')} /></Field>
        <Field label="Имя"><Input placeholder="Иван" value={f.first_name} onChange={set('first_name')} /></Field>
      </div>
      <div className="row2">
        <Field label="Отчество"><Input placeholder="Иванович" value={f.middle_name} onChange={set('middle_name')} /></Field>
        <Field label="Пол">
          <Select value={f.gender} onChange={set('gender')}>
            <option value="0">Женский</option><option value="1">Мужской</option><option value="2">Не указан</option>
          </Select>
        </Field>
      </div>
      <div className="row2">
        <Field label="Полис ОМС *" hint="6 цифр"><Input placeholder="100001" value={f.insuranceNum} onChange={set('insuranceNum')} /></Field>
        <Field label="Дата рождения"><Input type="date" value={f.birthDate} onChange={set('birthDate')} /></Field>
      </div>
      <Field label="Телефон"><Input placeholder="+375 (29) 000-00-00" value={f.phone} onChange={set('phone')} /></Field>
      <div style={{ display:'flex', gap:8, justifyContent:'flex-end', marginTop:4 }}>
        <button className="btn ghost" onClick={onClose}>Отмена</button>
        <button className="btn primary" onClick={save}>Сохранить</button>
      </div>
    </Modal>
  );
}

function AddDoctorModal({ specs, onSave, onClose, toast }) {
  const [f, setF] = React.useState({ last_name:'', first_name:'', middle_name:'', phone:'', specialization_id:'' });
  const set = k => e => setF(p => ({ ...p, [k]: e.target.value }));
  async function save() {
    try {
      await apiPost('/doctors', { ...f, specialization_id: parseInt(f.specialization_id) });
      toast('Врач добавлен', true);
      onSave();
    } catch(e) { toast(e.message, false); }
  }
  return (
    <Modal title="Добавить врача" onClose={onClose} width={500}>
      <div className="row2">
        <Field label="Фамилия"><Input placeholder="Петрова" value={f.last_name} onChange={set('last_name')} /></Field>
        <Field label="Имя"><Input placeholder="Ольга" value={f.first_name} onChange={set('first_name')} /></Field>
      </div>
      <div className="row2">
        <Field label="Отчество"><Input value={f.middle_name} onChange={set('middle_name')} /></Field>
        <Field label="Телефон"><Input placeholder="+375 (29) 100-00-00" value={f.phone} onChange={set('phone')} /></Field>
      </div>
      <Field label="Специализация">
        <Select value={f.specialization_id} onChange={set('specialization_id')}>
          <option value="">— выберите —</option>
          {specs.map(s => <option key={s.id} value={s.id}>{s.name}</option>)}
        </Select>
      </Field>
      <div style={{ display:'flex', gap:8, justifyContent:'flex-end' }}>
        <button className="btn ghost" onClick={onClose}>Отмена</button>
        <button className="btn primary" onClick={save}>Сохранить</button>
      </div>
    </Modal>
  );
}

function AddScheduleModal({ doctor, rooms, onSave, onClose, toast }) {
  const [f, setF] = React.useState({ dayOfWeek:'1', roomId:'', timeFrom:'09:00', timeTo:'17:00', slotDurationMin:'30', breakAfterMin:'5' });
  const set = k => e => setF(p => ({ ...p, [k]: e.target.value }));
  async function save() {
    try {
      await apiPost('/schedules', { doctorId: doctor.id, roomId: parseInt(f.roomId), dayOfWeek: parseInt(f.dayOfWeek), timeFrom: f.timeFrom, timeTo: f.timeTo, slotDurationMin: parseInt(f.slotDurationMin), breakAfterMin: parseInt(f.breakAfterMin) });
      toast('Расписание добавлено', true);
      onSave();
    } catch(e) { toast(e.message, false); }
  }
  return (
    <Modal title={`Расписание — ${shortName(doctor)}`} onClose={onClose} width={460}>
      <div className="row2">
        <Field label="День недели">
          <Select value={f.dayOfWeek} onChange={set('dayOfWeek')}>
            {DAYS.slice(1).map((d,i) => <option key={i+1} value={i+1}>{d}</option>)}
          </Select>
        </Field>
        <Field label="Кабинет">
          <Select value={f.roomId} onChange={set('roomId')}>
            <option value="">— выберите —</option>
            {rooms.map(r => <option key={r.id} value={r.id}>№ {r.number} ({r.floor} эт.)</option>)}
          </Select>
        </Field>
      </div>
      <div className="row2">
        <Field label="Начало"><Input type="time" value={f.timeFrom} onChange={set('timeFrom')} /></Field>
        <Field label="Конец"><Input type="time" value={f.timeTo} onChange={set('timeTo')} /></Field>
      </div>
      <div className="row2">
        <Field label="Длит. слота (мин)"><Input type="number" value={f.slotDurationMin} onChange={set('slotDurationMin')} /></Field>
        <Field label="Перерыв (мин)"><Input type="number" value={f.breakAfterMin} onChange={set('breakAfterMin')} /></Field>
      </div>
      <div style={{ display:'flex', gap:8, justifyContent:'flex-end' }}>
        <button className="btn ghost" onClick={onClose}>Отмена</button>
        <button className="btn primary" onClick={save}>Сохранить</button>
      </div>
    </Modal>
  );
}

function AddRoomModal({ clinicId, onSave, onClose, toast }) {
  const [f, setF] = React.useState({ number:'', floor:'1' });
  const set = k => e => setF(p => ({ ...p, [k]: e.target.value }));
  async function save() {
    try {
      await apiPost('/rooms', { number: f.number, floor: parseInt(f.floor), clinicId });
      toast('Кабинет добавлен', true);
      onSave();
    } catch(e) { toast(e.message, false); }
  }
  return (
    <Modal title="Добавить кабинет" onClose={onClose} width={380}>
      <div className="row2">
        <Field label="Номер кабинета"><Input placeholder="214А" value={f.number} onChange={set('number')} /></Field>
        <Field label="Этаж"><Input type="number" min="1" value={f.floor} onChange={set('floor')} /></Field>
      </div>
      <div style={{ display:'flex', gap:8, justifyContent:'flex-end' }}>
        <button className="btn ghost" onClick={onClose}>Отмена</button>
        <button className="btn primary" onClick={save}>Сохранить</button>
      </div>
    </Modal>
  );
}

function ApptDetailModal({ appt, patients, doctors, onClose, onChangeStatus, toast }) {
  const pat = patients.find(p => p.id === appt.patientId);
  const doc = doctors.find(d => d.id === appt.doctorId);
  const [notes, setNotes] = React.useState(appt.notes || '');
  async function saveNotes() {
    try {
      await apiPatch(`/appointments/${appt.id}/notes`, { notes });
      toast('Заметки сохранены', true);
    } catch(e) { toast(e.message, false); }
  }
  return (
    <Modal title="Запись на приём" onClose={onClose} width={420}>
      <div style={{ display:'flex', gap:10, marginBottom:14 }}>
        <Av p={pat} size={40} fs={14} />
        <div>
          <div style={{ fontWeight:600 }}>{fullName(pat)}</div>
          <div style={{ fontSize:12, color:'var(--ink-3)' }}>{appt.scheduledAt}</div>
          <div style={{ fontSize:12, color:'var(--ink-3)' }}>Врач: {shortName(doc)}</div>
        </div>
      </div>
      <div style={{ display:'flex', gap:6, marginBottom:14 }}>
        {[['Подтверждена',0],['Завершена',1],['Отменена',2]].map(([l,v]) => (
          <button key={v} className={`btn${appt.status===v?' primary':' ghost'}`} style={{ flex:1, justifyContent:'center', fontSize:12 }}
            onClick={() => onChangeStatus(appt.id, v)}>{l}</button>
        ))}
      </div>
      <Field label="Заметки">
        <textarea className="input-el" rows={3} value={notes} onChange={e => setNotes(e.target.value)} style={{ resize:'vertical' }} />
      </Field>
      <div style={{ display:'flex', gap:8, justifyContent:'flex-end' }}>
        <button className="btn ghost" onClick={onClose}>Закрыть</button>
        <button className="btn primary" onClick={saveNotes}>Сохранить заметки</button>
      </div>
    </Modal>
  );
}

Object.assign(window, {
  DashboardView, CalendarView, PatientsView, DoctorsView, RoomsView,
  NewAppointmentView, AddPatientModal, AddDoctorModal, AddScheduleModal,
  AddRoomModal, ApptDetailModal, Modal, ConfirmModal,
});
