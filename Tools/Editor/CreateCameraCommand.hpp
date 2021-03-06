#ifndef CREATECAMERACOMMAND_HPP
#define CREATECAMERACOMMAND_HPP

#include "Command.hpp"

namespace ae3d
{
    class GameObject;
}

class CreateCameraCommand : public CommandBase
{
public:
    explicit CreateCameraCommand( class SceneWidget* sceneWidget );
    void Execute() override;
    void Undo() override;

private:
    SceneWidget* sceneWidget = nullptr;
    ae3d::GameObject* gameObject = nullptr;
};

#endif
