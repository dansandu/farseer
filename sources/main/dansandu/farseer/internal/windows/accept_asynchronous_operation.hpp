#pragma once

#include "dansandu/farseer/common.hpp"
#include "dansandu/farseer/internal/windows/asynchronous_operation.hpp"

namespace dansandu::farseer::internal::windows::accept_asynchronous_operation
{

std::unique_ptr<dansandu::farseer::internal::windows::asynchronous_operation::AsynchronousOperation>
createAcceptAsynchronousOperation(dansandu::farseer::internal::sequencer::Sequencer<SocketServiceId>& sequencer,
                                  const SocketServiceId listeningServiceId);

}
