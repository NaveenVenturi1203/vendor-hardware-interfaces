/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CAR_EVS_APP_VEHICLELISTENER_H
#define CAR_EVS_APP_VEHICLELISTENER_H

#include "EvsStateControl.h"

#include <IVhalClient.h>

#include <vector>

/*
 * This class listens for asynchronous updates from the Vehicle HAL.  While the EVS
 * applications is active, it can poll the vehicle state directly.  However, when it goes to
 * sleep, we need these notifications to bring it active again.
 */
class EvsVehicleListener final :
      public android::frameworks::automotive::vhal::ISubscriptionCallback {
public:
    void onPropertyEvent([[maybe_unused]] const std::vector<
                         std::unique_ptr<android::frameworks::automotive::vhal::IHalPropValue>>&
                                 values) override {
        {
            // Our use case is so simple, we don't actually need to update a variable,
            // but the docs seem to say we have to take the lock anyway to keep
            // the condition variable implementation happy.
            std::lock_guard<std::mutex> g(mLock);
        }
        mEventCond.notify_one();
        return;
    }

    void onPropertySetError(
            [[maybe_unused]] const std::vector<android::frameworks::automotive::vhal::HalPropError>&
                    errors) override {
        return;
    }

    bool waitForEvents(int timeout_ms) {
        std::unique_lock<std::mutex> g(mLock);
        std::cv_status result = mEventCond.wait_for(g, std::chrono::milliseconds(timeout_ms));
        return (result == std::cv_status::no_timeout);
    }

    void run(EvsStateControl* pStateController) {
        mRunning = true;
        while (mRunning) {
            // Wait until we have an event to which to react
            // (wake up and validate our current state "just in case" every so often)
            waitForEvents(5000);

            // If we were delivered an event (or it's been a while) update as necessary
            EvsStateControl::Command cmd = {
                    .operation = EvsStateControl::Op::CHECK_VEHICLE_STATE,
                    .arg1 = 0,
                    .arg2 = 0,
            };
            pStateController->postCommand(cmd);
        }
    }

    void terminate() {
        mRunning = false;
    }

private:
    std::mutex mLock;
    std::condition_variable mEventCond;
    bool mRunning;
};

#endif  // CAR_EVS_APP_VEHICLELISTENER_H
