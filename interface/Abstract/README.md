# interface/Abstract/

Чистые интерфейсы репозиториев — то, что видят сервисы. Реализаций здесь нет.

Один файл — один интерфейс, по одному на доменную сущность из
[`../../models`](../../models):

| Интерфейс            | Сущность         |
|----------------------|------------------|
| `IClinic`            | `Clinic`         |
| `IRoom`              | `Room`           |
| `ISpecialization`    | `Specialization` |
| `IDoctor`            | `Doctor`         |
| `IPatient`           | `Patient`        |
| `ISchedule`          | `Schedule`       |
| `IAppointment`       | `Appointment`    |

## Общая форма

```cpp
class IXxx {
public:
    virtual ~IXxx() = default;
    virtual void save(Xxx& obj) = 0;            // id == -1 → создать, иначе обновить
    virtual std::optional<Xxx> findById(int id) const = 0;
    virtual std::vector<Xxx>   findAll() = 0;
    virtual std::vector<Xxx>   findBy(const std::function<bool(const Xxx&)>&) = 0;
    virtual bool               removeBy(int id) = 0;
};
```

Мелкие отличия по сущностям (например, у `IAppointment` метод
называется `remove`, а не `removeBy`; у некоторых добавлены
доменно-специфичные поисковые методы) — смотреть в самих заголовках.

## Контракт `save`

- `obj.id == -1` → реализация присваивает новый id и пишет.
- `obj.id != -1` → upsert: если такая запись есть — обновить,
  иначе — вставить с этим id (важно для импорта бэкапов из
  [`../../services/BackupService.h`](../../services/BackupService.h)).
