-- UAL1_Standard 스켈레탈 메시 액터에 연결한다.
-- PIE에서 A 공격, H 피격, R 재피격 재시작 토글.
local pack = "Assets/Models/UAL1/UAL1_Standard.fbx"
local sm
local attackRequested = false
local hitRequested = false
local restartHit = false
local lastState

local function Build()
    attackRequested = false
    hitRequested = false
    local graph = obj:CreateAnimGraph()

    local locomotion = graph:AddSequenceState("Locomotion", pack, "Idle_Loop", true)
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
