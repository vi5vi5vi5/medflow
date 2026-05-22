// httplib тянет <winsock2.h>/<windows.h> — должен идти ПЕРВЫМ, до любых
// заголовков, в которых может встретиться `using namespace std;`,
// иначе ловим неоднозначность std::byte vs ::byte (rpcndr.h).
#include "../requirements/httplib.h"

#include "InsuranceValidator.h"

bool validateInsuranceNumRemote(const std::string& insuranceNum) {
    httplib::Client cli("http://82.40.57.187:8123");
    cli.set_connection_timeout(3, 0);
    cli.set_read_timeout(3, 0);
    auto res = cli.Get("/check?policy=" + insuranceNum);
    return res && res->status == 200;
}
