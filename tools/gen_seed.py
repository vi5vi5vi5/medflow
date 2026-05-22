"""
Генератор тестового бэкапа для MedFlow.

Соблюдает доменные инварианты, которые проверяют сервисы:
  - Patient.clinic_id указывает на существующую клинику.
  - Doctor → Schedule(s) → Room → Clinic: доктор «работает» в клинике,
    только если у него есть Schedule в кабинете этой клиники.
  - Для каждого приёма: doctor имеет Schedule в room.clinicId == patient.clinic_id
    на нужный день недели, и время попадает в один из слотов
    (timeFrom + k*(slotDurationMin + breakAfterMin)).
  - Нет пересечений приёмов для одного доктора и для одного пациента.
  - У одного доктора не больше одного Schedule на (день_недели), чтобы
    не падал ScheduleService::setNewSchedule при пере-генерации.
  - createdAt < scheduledAt; статус согласован с датой.

Выход: ../seed.bck (UTF-8, pretty JSON).
"""

import json
import os
import random
from datetime import date, timedelta


SEED = 42
TODAY = date(2026, 5, 21)
DATE_PAST_DAYS = 30
DATE_FUTURE_DAYS = 30
N_PATIENTS = 100

CLINICS = [
    {
        "name": "Медицинский центр «Пресня»",
        "address": "г. Москва, ул. Красная Пресня, д. 12",
        "phone": "+7 (495) 783-21-40",
    },
    {
        "name": "Поликлиника «Соколиная гора»",
        "address": "г. Москва, шоссе Энтузиастов, д. 62",
        "phone": "+7 (495) 365-87-12",
    },
    {
        "name": "Клиника «Профсоюзная»",
        "address": "г. Москва, ул. Профсоюзная, д. 76А",
        "phone": "+7 (499) 128-54-66",
    },
]

SPECIALIZATIONS = [
    "Терапевт",
    "Кардиолог",
    "Невролог",
    "Офтальмолог",
    "Хирург",
    "Эндокринолог",
    "Оториноларинголог",
]

DOCTORS_PER_SPEC = {
    "Терапевт": 4,
    "Кардиолог": 2,
    "Невролог": 2,
    "Офтальмолог": 2,
    "Хирург": 2,
    "Эндокринолог": 2,
    "Оториноларинголог": 2,
}

LAST_MALE = [
    "Иванов", "Петров", "Сидоров", "Кузнецов", "Соколов", "Морозов",
    "Новиков", "Фёдоров", "Орлов", "Лебедев", "Семёнов", "Павлов",
    "Тимофеев", "Андреев", "Григорьев", "Михайлов", "Алексеев", "Степанов",
    "Никитин", "Захаров", "Беляев", "Зайцев", "Богданов", "Гусев",
    "Поляков", "Виноградов", "Воробьёв", "Ковалёв", "Дмитриев", "Тарасов",
    "Соловьёв", "Якушев", "Карпов", "Бабкин", "Сорокин", "Громов",
    "Жуков", "Крылов", "Кудрявцев", "Носков",
]
LAST_FEMALE = [f[:-1] + "а" if f.endswith("в") else f + "а" for f in LAST_MALE]

FIRST_MALE = [
    "Александр", "Алексей", "Андрей", "Антон", "Артём", "Борис",
    "Вадим", "Валентин", "Валерий", "Василий", "Виктор", "Владимир",
    "Вячеслав", "Геннадий", "Глеб", "Григорий", "Денис", "Дмитрий",
    "Евгений", "Егор", "Иван", "Игорь", "Илья", "Кирилл",
    "Константин", "Леонид", "Максим", "Михаил", "Никита", "Николай",
    "Олег", "Павел", "Пётр", "Роман", "Сергей", "Станислав",
    "Степан", "Тимур", "Фёдор", "Юрий",
]
FIRST_FEMALE = [
    "Александра", "Алина", "Алла", "Анастасия", "Анна", "Валентина",
    "Валерия", "Вера", "Виктория", "Галина", "Дарья", "Евгения",
    "Екатерина", "Елена", "Жанна", "Зинаида", "Ирина", "Ксения",
    "Лариса", "Лидия", "Любовь", "Людмила", "Маргарита", "Марина",
    "Мария", "Надежда", "Наталья", "Нина", "Оксана", "Ольга",
    "Полина", "Раиса", "Светлана", "София", "Тамара", "Татьяна",
    "Ульяна", "Юлия", "Яна",
]
MIDDLE_MALE = [
    "Александрович", "Алексеевич", "Андреевич", "Артёмович", "Борисович",
    "Валерьевич", "Васильевич", "Викторович", "Владимирович", "Вячеславович",
    "Геннадьевич", "Григорьевич", "Денисович", "Дмитриевич", "Евгеньевич",
    "Иванович", "Игоревич", "Ильич", "Кириллович", "Константинович",
    "Леонидович", "Максимович", "Михайлович", "Никитич", "Николаевич",
    "Олегович", "Павлович", "Петрович", "Романович", "Сергеевич",
    "Станиславович", "Степанович", "Фёдорович", "Юрьевич",
]
MIDDLE_FEMALE = [
    "Александровна", "Алексеевна", "Андреевна", "Артёмовна", "Борисовна",
    "Валерьевна", "Васильевна", "Викторовна", "Владимировна", "Вячеславовна",
    "Геннадьевна", "Григорьевна", "Денисовна", "Дмитриевна", "Евгеньевна",
    "Ивановна", "Игоревна", "Ильинична", "Кирилловна", "Константиновна",
    "Леонидовна", "Максимовна", "Михайловна", "Никитична", "Николаевна",
    "Олеговна", "Павловна", "Петровна", "Романовна", "Сергеевна",
    "Станиславовна", "Степановна", "Фёдоровна", "Юрьевна",
]

MOBILE_PREFIXES = [
    "903", "905", "906", "909", "910", "915", "916", "917", "919",
    "925", "926", "929", "963", "964", "965", "967", "968", "977", "999",
]

TIME_WINDOWS = [
    ("08:00", "13:00"),
    ("09:00", "14:00"),
    ("09:00", "15:00"),
    ("10:00", "16:00"),
    ("12:00", "18:00"),
    ("13:00", "19:00"),
    ("14:00", "20:00"),
]
SLOT_DURATIONS = [20, 25, 30]
BREAK_DURATIONS = [0, 5, 5, 10]


def mobile_phone(used):
    while True:
        prefix = random.choice(MOBILE_PREFIXES)
        rest = f"{random.randint(0, 999):03d}-{random.randint(0, 99):02d}-{random.randint(0, 99):02d}"
        phone = f"+7 {prefix} {rest}"
        if phone not in used:
            used.add(phone)
            return phone


def insurance_oms(used):
    while True:
        ins = "".join(str(random.randint(0, 9)) for _ in range(16))
        if ins not in used:
            used.add(ins)
            return ins


def birth_date(min_age=18, max_age=85):
    days = random.randint(min_age * 365, max_age * 365)
    return (TODAY - timedelta(days=days)).isoformat()


def t2m(hhmm):
    h, m = hhmm.split(":")
    return int(h) * 60 + int(m)


def m2t(m):
    return f"{m // 60:02d}:{m % 60:02d}"


def main():
    random.seed(SEED)
    used_phones = set()
    used_insurance = set()

    # Клиники
    clinics = [{"id": i, **c} for i, c in enumerate(CLINICS)]

    # Специализации
    specializations = [{"id": i, "name": n} for i, n in enumerate(SPECIALIZATIONS)]

    # Кабинеты: 4-6 на клинику
    rooms = []
    rid = 0
    for c in clinics:
        n_rooms = random.randint(4, 6)
        used_numbers = set()
        for _ in range(n_rooms):
            while True:
                floor = random.randint(1, 3)
                number = f"{floor}{random.randint(10, 30):02d}"
                if number not in used_numbers:
                    used_numbers.add(number)
                    break
            rooms.append({
                "id": rid,
                "number": number,
                "floor": floor,
                "clinicId": c["id"],
            })
            rid += 1

    rooms_by_clinic = {c["id"]: [r for r in rooms if r["clinicId"] == c["id"]] for c in clinics}

    # Доктора
    doctors = []
    did = 0
    for spec in specializations:
        for _ in range(DOCTORS_PER_SPEC[spec["name"]]):
            male = random.random() < 0.5
            if male:
                last = random.choice(LAST_MALE)
                first = random.choice(FIRST_MALE)
                middle = random.choice(MIDDLE_MALE)
            else:
                last = random.choice(LAST_FEMALE)
                first = random.choice(FIRST_FEMALE)
                middle = random.choice(MIDDLE_FEMALE)
            doctors.append({
                "id": did,
                "first_name": first,
                "last_name": last,
                "middle_name": middle,
                "phone": mobile_phone(used_phones),
                "specialization_id": spec["id"],
            })
            did += 1

    # Расписания: каждый доктор работает в 1-2 клиниках, 3-5 дней Пн-Пт.
    # Один день недели — одна клиника (чтобы не было overlapping schedules).
    schedules = []
    sid = 0
    for doc in doctors:
        n_clinics = random.choices([1, 2], weights=[0.55, 0.45])[0]
        chosen_clinics = random.sample([c["id"] for c in clinics], n_clinics)
        n_days = random.randint(3, 5)
        working_days = sorted(random.sample(range(1, 6), n_days))  # Mon..Fri
        # round-robin clinic assignment by day
        for i, day in enumerate(working_days):
            clinic_id = chosen_clinics[i % n_clinics]
            room = random.choice(rooms_by_clinic[clinic_id])
            tf, tt = random.choice(TIME_WINDOWS)
            schedules.append({
                "id": sid,
                "doctorId": doc["id"],
                "roomId": room["id"],
                "dayOfWeek": day,
                "timeFrom": tf,
                "timeTo": tt,
                "slotDurationMin": random.choice(SLOT_DURATIONS),
                "breakAfterMin": random.choice(BREAK_DURATIONS),
            })
            sid += 1

    # Пациенты: равномерно по 3 клиникам
    patients = []
    pid = 0
    for i in range(N_PATIENTS):
        female = random.random() < 0.55
        if female:
            last = random.choice(LAST_FEMALE)
            first = random.choice(FIRST_FEMALE)
            middle = random.choice(MIDDLE_FEMALE)
            gender = 0
        else:
            last = random.choice(LAST_MALE)
            first = random.choice(FIRST_MALE)
            middle = random.choice(MIDDLE_MALE)
            gender = 1
        patients.append({
            "id": pid,
            "insuranceNum": insurance_oms(used_insurance),
            "first_name": first,
            "last_name": last,
            "middle_name": middle,
            "gender": gender,
            "birthDate": birth_date(),
            "phone": mobile_phone(used_phones),
            "clinic_id": i % len(clinics),
        })
        pid += 1

    # Индекс: (clinic_id, day_of_week) -> [schedules]
    rooms_by_id = {r["id"]: r for r in rooms}
    sched_index = {}
    for s in schedules:
        key = (rooms_by_id[s["roomId"]]["clinicId"], s["dayOfWeek"])
        sched_index.setdefault(key, []).append(s)

    # Занятые минуты по доктору/пациенту на дату
    doc_busy = {}
    pat_busy = {}

    NOTES = [
        "", "", "", "",
        "Первичный приём",
        "Повторный приём",
        "Жалобы на головную боль",
        "Плановый осмотр",
        "Контрольный визит",
        "Профилактический осмотр",
        "Острая боль",
        "Диспансеризация",
    ]

    appointments = []
    aid = 0
    for patient in patients:
        n_appts = random.choices([1, 2, 3, 4, 5], weights=[12, 28, 30, 20, 10])[0]
        tries = 0
        placed = 0
        while placed < n_appts and tries < n_appts * 10:
            tries += 1
            offset = random.randint(-DATE_PAST_DAYS, DATE_FUTURE_DAYS)
            d = TODAY + timedelta(days=offset)
            dow = d.isoweekday()
            cands = sched_index.get((patient["clinic_id"], dow), [])
            if not cands:
                continue
            sched = random.choice(cands)
            slot = sched["slotDurationMin"]
            step = slot + sched["breakAfterMin"]
            tf = t2m(sched["timeFrom"])
            tt = t2m(sched["timeTo"])
            starts = list(range(tf, tt - slot + 1, step))
            random.shuffle(starts)
            date_str = d.isoformat()
            db = doc_busy.setdefault((sched["doctorId"], date_str), set())
            pb = pat_busy.setdefault((patient["id"], date_str), set())
            chosen_start = None
            for st in starts:
                rng = range(st, st + slot)
                if any(m in db for m in rng):
                    continue
                if any(m in pb for m in rng):
                    continue
                chosen_start = st
                for m in rng:
                    db.add(m)
                    pb.add(m)
                break
            if chosen_start is None:
                continue

            if offset < -1:
                status = random.choices([1, 2], weights=[82, 18])[0]
            elif offset == -1 or offset == 0:
                status = random.choices([0, 1, 2], weights=[40, 50, 10])[0]
            else:
                status = random.choices([0, 2], weights=[92, 8])[0]

            created_offset = random.randint(1, 30)
            created_d = d - timedelta(days=created_offset)
            if created_d > TODAY:
                created_d = TODAY - timedelta(days=random.randint(0, 3))
            created_at = (
                f"{created_d.isoformat()} "
                f"{random.randint(8, 19):02d}:{random.choice(['00', '15', '30', '45'])}"
            )

            appointments.append({
                "id": aid,
                "patientId": patient["id"],
                "doctorId": sched["doctorId"],
                "scheduledAt": f"{date_str} {m2t(chosen_start)}",
                "status": status,
                "notes": random.choice(NOTES),
                "createdAt": created_at,
            })
            aid += 1
            placed += 1

    # ---------- Самопроверка ----------
    issues = []
    clinic_ids = {c["id"] for c in clinics}
    spec_ids = {s["id"] for s in specializations}
    doc_ids = {d["id"] for d in doctors}
    patient_ids = {p["id"] for p in patients}

    for r in rooms:
        if r["clinicId"] not in clinic_ids:
            issues.append(f"room {r['id']}: orphan clinicId")
    for d in doctors:
        if d["specialization_id"] not in spec_ids:
            issues.append(f"doctor {d['id']}: orphan spec")
    for p in patients:
        if p["clinic_id"] not in clinic_ids:
            issues.append(f"patient {p['id']}: orphan clinic")
    for s in schedules:
        if s["doctorId"] not in doc_ids:
            issues.append(f"schedule {s['id']}: orphan doctor")
        if s["roomId"] not in rooms_by_id:
            issues.append(f"schedule {s['id']}: orphan room")

    # Все расписания одного доктора на один день — не должны пересекаться
    doc_day_sched = {}
    for s in schedules:
        doc_day_sched.setdefault((s["doctorId"], s["dayOfWeek"]), []).append(s)
    for key, lst in doc_day_sched.items():
        if len(lst) > 1:
            issues.append(f"doctor {key[0]} dow {key[1]}: {len(lst)} overlapping schedules")

    # Каждый приём — есть schedule в клинике пациента на нужный день, время в окне
    patients_by_id = {p["id"]: p for p in patients}
    sched_by_doc = {}
    for s in schedules:
        sched_by_doc.setdefault(s["doctorId"], []).append(s)
    for a in appointments:
        p = patients_by_id[a["patientId"]]
        d = date.fromisoformat(a["scheduledAt"][:10])
        appt_min = t2m(a["scheduledAt"][11:16])
        ok = False
        for s in sched_by_doc.get(a["doctorId"], []):
            if s["dayOfWeek"] != d.isoweekday():
                continue
            if rooms_by_id[s["roomId"]]["clinicId"] != p["clinic_id"]:
                continue
            if t2m(s["timeFrom"]) <= appt_min < t2m(s["timeTo"]):
                ok = True
                break
        if not ok:
            issues.append(
                f"appt {a['id']}: no matching schedule "
                f"doc={a['doctorId']} clinic={p['clinic_id']} at {a['scheduledAt']}"
            )

    if issues:
        print(f"VALIDATION FAILED: {len(issues)} issue(s)")
        for i in issues[:30]:
            print(" -", i)
        raise SystemExit(1)

    out = {
        "clinics": clinics,
        "specializations": specializations,
        "rooms": rooms,
        "doctors": doctors,
        "patients": patients,
        "schedules": schedules,
        "appointments": appointments,
    }

    out_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "seed.bck")
    )
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2, sort_keys=True)

    print(f"OK → {out_path}")
    print(
        f"  clinics={len(clinics)} specs={len(specializations)} rooms={len(rooms)} "
        f"doctors={len(doctors)} patients={len(patients)} "
        f"schedules={len(schedules)} appts={len(appointments)}"
    )
    past = sum(1 for a in appointments if a["scheduledAt"][:10] < TODAY.isoformat())
    today = sum(1 for a in appointments if a["scheduledAt"][:10] == TODAY.isoformat())
    future = sum(1 for a in appointments if a["scheduledAt"][:10] > TODAY.isoformat())
    print(f"  appts split: past={past} today={today} future={future}")


if __name__ == "__main__":
    main()
