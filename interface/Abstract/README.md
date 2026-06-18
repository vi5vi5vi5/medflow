# interface/Abstract/ — контракты репозиториев

Чистые интерфейсы (`virtual ... = 0`), на которые опираются сервисы.
Реализаций здесь нет — только «форма». По одному файлу на сущность из
[`../../models`](../../models):

| Интерфейс | Сущность |
|---|---|
| `IClinic` | `Clinic` |
| `IRoom` | `Room` |
| `ISpecialization` | `Specialization` |
| `IDoctor` | `Doctor` |
| `IPatient` | `Patient` |
| `ISchedule` | `Schedule` |
| `IAppointment` | `Appointment` |

## Канонический набор методов

Пять методов, которые есть у большинства интерфейсов:

```cpp
class IXxx {
public:
    virtual ~IXxx() = default;
    virtual void                    save(Xxx& obj)                                    = 0;
    virtual std::optional<Xxx>      findById(int id) const                            = 0;
    virtual std::vector<Xxx>        findAll() const                                   = 0;
    virtual std::vector<Xxx>        findBy(const std::function<bool(const Xxx&)>&) const = 0;
    virtual bool                    removeBy(int id)                                  = 0;
};
```

- `findById` → `std::nullopt`, если записи нет.
- `findBy(predicate)` принимает **произвольную C++-лямбду** — это, а не SQL-WHERE,
  и есть основной способ фильтрации. Цена — полный проход (см. реализации).
- `removeBy` → `true`, если что-то удалилось; `false`, если такого id не было.

## Контракт `save` (важно для бэкапов)

`save` — это upsert, поведение зависит от `obj.id`:

- `obj.id == -1` → реализация присваивает **новый** id и вставляет; присвоенный id
  пишется обратно в `obj.id` (по ссылке).
- `obj.id != -1` → вставить/перезаписать запись **с этим конкретным id**.

Вторая ветка критична для импорта: [`../../services/BackupService.h`](../../services/BackupService.h)
заливает снапшот, сохраняя исходные id, — поэтому связи между сущностями
(`clinic_id`, `doctor_id`, …) переживают восстановление.

## Отклонения от канона (реальные, по коду)

Интерфейсы исторически не идеально единообразны — это нужно держать в голове:

| Интерфейс | Отличие |
|---|---|
| `IClinic`, `IRoom`, `IDoctor`, `ISchedule` | канон без изменений |
| `IPatient` | `findAll()` и `findBy()` объявлены **non-const** (у остальных — `const`) |
| `IAppointment` | удаление называется **`remove(int)`**, а не `removeBy(int)` |
| `ISpecialization` | **нет** `findById` и **нет** `findBy(predicate)`. Вместо них две перегрузки: `findBy(int id)` и `findBy(std::string name)`, обе возвращают `std::optional<Specialization>`. Поиск по имени — по подстроке, возвращает первое совпадение |

Доменно-специфичных поисковых методов (типа «по дню недели») в интерфейсах нет —
такая логика собирается в сервисах поверх `findBy(predicate)`.

## Реализации

Два адаптера под эти контракты: [`../InMemory`](../InMemory) (для тестов/проб) и
[`../sqlite`](../sqlite) (боевой). Выбор — в точке сборки, см.
[`../README.md`](../README.md).
