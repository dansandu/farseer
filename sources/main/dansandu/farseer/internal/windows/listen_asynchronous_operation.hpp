#pragma once

#include "dansandu/farseer/internal/sequencer.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"

namespace dansandu::farseer::internal::windows::listen_asynchronous_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation>
createListenAsynchronousOperation(dansandu::farseer::internal::sequencer::Sequencer<SocketServiceId>& sequencer,
                                  const std::wstring& ipAddress, const int port,
                                  ConnectionCallbackType connectionCallback);

}
