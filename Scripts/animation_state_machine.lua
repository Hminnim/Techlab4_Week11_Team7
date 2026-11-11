-- UAL1_Standard 스켈레탈 메시 액터에 연결한다.
-- PIE에서 A 공격, H 피격, R 재피격 재시작 토글.
-- 이동 컴포넌트가 없는 액터에서는 W 걷기, Shift+W 달리기를 제자리에서 미리 본다.
local pack = "Assets/Models/UAL1/UAL1_Standard.fbx"
local walkSpeed = 1.0
local sprintSpeed = 3.0
local sm
local locomotionState
local previewSpeed
local useMovementVelocity
local attackRequested = false
local hitRequested = false
local restartHit = false
local lastState

local function Build()
    attackRequested = false
    hitRequested = false
    local graph = obj:CreateAnimGraph()

    local locomotion = graph:AddBlendSpaceState("Locomotion", pack, {
        { take = "Idle_Loop", x = 0 },
        { take = "Walk_Loop", x = walkSpeed },
        { take = "Sprint_Loop", x = sprintSpeed }
    })
    local attack = graph:AddSequenceState("Attack", pack, "Sword_Attack", false)
    local hit = graph:AddSequenceState("Hit", pack, "Hit_Chest", false)
    assert(locomotion >= 0 and attack >= 0 and hit >= 0)

    assert(graph:AddTransition(locomotion, attack, function()
        return attackRequested
    end))
    assert(graph:AddTransition(attack, locomotion, function()
        return graph:IsStateFinished(attack)
    end))
    assert(graph:AddAnyStateTransition(hit, function()
        return hitRequested
    end))
    assert(graph:AddTransition(hit, locomotion, function()
        return graph:IsStateFinished(hit)
    end))
    assert(graph:SetStateRestartOnReentry(hit, restartHit))
    assert(graph:Start(locomotion))
    sm = graph
    locomotionState = locomotion
    previewSpeed = 0
    useMovementVelocity = obj:HasComponent("UMovementComponent")
    lastState = sm:GetCurrentStateName()
    print("[Lua Animation] " .. lastState)

    return hit
end

local hitState

function BeginPlay()
    hitState = Build()
end

function OnReload()
    hitState = Build()
end

function Tick(dt)
    if not sm or not sm:IsValid() then return end

    local speed
    if useMovementVelocity then
        local velocity = obj.Velocity
        speed = math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y)
    else
        local target = IsKeyDown("W") and (IsKeyDown("shift") and sprintSpeed or walkSpeed) or 0
        local step = sprintSpeed * dt
        previewSpeed = previewSpeed + math.max(-step, math.min(step, target - previewSpeed))
        speed = previewSpeed
    end
    assert(sm:SetBlendSpaceInput(locomotionState, speed))

    attackRequested = IsKeyPressed("A")
    hitRequested = IsKeyPressed("H")
    if IsKeyPressed("R") then
        restartHit = not restartHit
        assert(sm:SetStateRestartOnReentry(hitState, restartHit))
        print("[Lua Animation] Restart Hit: " .. tostring(restartHit))
    end

    local currentState = sm:GetCurrentStateName()
    if currentState ~= lastState then
        lastState = currentState
        print("[Lua Animation] " .. currentState)
    end
end

function EndPlay()
    sm = nil
end

function OnAnimNotify(notifyName, clipName)
    print("[Lua AnimNotify] '" .. tostring(notifyName) .. "' from clip '" .. tostring(clipName) .. "'")
end

function OnAnimNotifyBegin(notifyName, clipName, duration)
    print(string.format("[Lua AnimNotifyState] Begin '%s' from clip '%s' (Duration: %.3fs)",
        tostring(notifyName), tostring(clipName), tonumber(duration) or 0))
end

-- function OnAnimNotifyTick(notifyName, clipName, dt)
-- end

function OnAnimNotifyEnd(notifyName, clipName)
    print(string.format("[Lua AnimNotifyState] End '%s' from clip '%s'",
        tostring(notifyName), tostring(clipName)))
end
