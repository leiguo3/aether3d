#include "CameraInspector.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <QBoxLayout>
#include <QColorDialog>
#include <QTableWidget>
#include <QLabel>
#include <QComboBox>
#include <QString>
#include <QHeaderView>
#include <QIntValidator>
#include <QPushButton>
#include <QLineEdit>
#include "MainWindow.hpp"
#include "GameObject.hpp"
#include "CameraComponent.hpp"
#include "Quaternion.hpp"
#include "System.hpp"

using namespace ae3d;

QString FloatToQString( float f )
{
    std::ostringstream out;
    out << std::setprecision( 6 ) << f;
    return QString::fromStdString( out.str() );
}

QString IntToQString( int i )
{
    std::ostringstream out;
    out << i;
    return QString::fromStdString( out.str() );
}

void CameraInspector::Init( QWidget* mainWindow )
{
    ortho = new QTableWidget( 1, 4 );
    //ortho->horizontalHeader()->setDefaultSectionSize( 15 );
    ortho->setItem( 0, 0, new QTableWidgetItem() );
    ortho->setItem( 0, 1, new QTableWidgetItem() );
    ortho->setItem( 0, 2, new QTableWidgetItem() );
    ortho->setItem( 0, 3, new QTableWidgetItem() );
    ortho->setHorizontalHeaderLabels( QString("Width;Height;Near;Far").split(";") );
    ortho->setVerticalHeaderLabels( QString("Orthographic").split(";") );

    persp = new QTableWidget( 1, 4 );
    persp->setItem( 0, 0, new QTableWidgetItem() );
    persp->setItem( 0, 1, new QTableWidgetItem() );
    persp->setItem( 0, 2, new QTableWidgetItem() );
    persp->setItem( 0, 3, new QTableWidgetItem() );
    persp->setHorizontalHeaderLabels( QString("Fov;Aspect;Near;Far").split(";") );
    persp->setVerticalHeaderLabels( QString("Perspective").split(";") );

    clearColorTable = new QTableWidget( 1, 3 );
    clearColorTable->setItem( 0, 0, new QTableWidgetItem() );
    clearColorTable->setItem( 0, 1, new QTableWidgetItem() );
    clearColorTable->setItem( 0, 2, new QTableWidgetItem() );
    clearColorTable->setHorizontalHeaderLabels( QString("Red;Green;Blue").split(";") );
    //clearColorTable->setVerticalHeaderLabels( QString("Clear Color").split(";") );

    QLabel* componentName = new QLabel("Camera");
    QLabel* clearTitle = new QLabel("Clear");
    QLabel* clearColorTitle = new QLabel("Clear Color");
    QLabel* projectionTitle = new QLabel("Projection");
    QLabel* orderTitle = new QLabel("Render order");

    orderInput = new QLineEdit();
    orderInput->setMaxLength( 3 );
    orderInput->setMaximumWidth( 40 );
    orderInput->setValidator( new QIntValidator() );

    QWidget* orderWidget = new QWidget();
    QBoxLayout* orderLayout = new QBoxLayout( QBoxLayout::LeftToRight );
    orderLayout->addWidget( orderTitle );
    orderLayout->addWidget( orderInput );
    orderWidget->setLayout( orderLayout );

    clearFlagsBox = new QComboBox();
    clearFlagsBox->addItem("Color and depth");
    clearFlagsBox->addItem("Depth");
    clearFlagsBox->addItem("Don't clear");

    projectionBox = new QComboBox();
    projectionBox->addItem("Orthographic");
    projectionBox->addItem("Perspective");

    QBoxLayout* clearLayout = new QBoxLayout( QBoxLayout::LeftToRight );
    clearLayout->setContentsMargins( 1, 1, 1, 1 );
    clearLayout->addWidget( clearTitle );
    clearLayout->addWidget( clearFlagsBox );
    QWidget* clearWidget = new QWidget();
    clearWidget->setLayout( clearLayout );

    QBoxLayout* clearColorLayout = new QBoxLayout( QBoxLayout::LeftToRight );
    clearColorLayout->setContentsMargins( 1, 1, 1, 1 );
    clearColorLayout->addWidget( clearColorTitle );
    clearColorLayout->addWidget( clearColorTable );
    QWidget* clearColorWidget = new QWidget();
    clearColorWidget->setLayout( clearColorLayout );

    QBoxLayout* projectionLayout = new QBoxLayout( QBoxLayout::LeftToRight );
    projectionLayout->setContentsMargins( 1, 1, 1, 1 );
    projectionLayout->addWidget( projectionTitle );
    projectionLayout->addWidget( projectionBox );
    QWidget* projectionWidget = new QWidget();
    projectionWidget->setLayout( projectionLayout );

    removeButton = new QPushButton("remove");
    QBoxLayout* headerLayout = new QBoxLayout( QBoxLayout::LeftToRight );
    headerLayout->addWidget( componentName );
    headerLayout->addWidget( removeButton );
    QWidget* header = new QWidget();
    header->setLayout( headerLayout );

    QBoxLayout* inspectorLayout = new QBoxLayout( QBoxLayout::TopToBottom );
    inspectorLayout->setContentsMargins( 1, 1, 1, 1 );
    inspectorLayout->addWidget( header );
    inspectorLayout->addWidget( orderWidget );
    inspectorLayout->addWidget( clearWidget );
    inspectorLayout->addWidget( clearColorWidget );
    inspectorLayout->addWidget( projectionWidget );
    inspectorLayout->addWidget( ortho );
    inspectorLayout->addWidget( persp );

    root = new QWidget();
    root->setLayout( inspectorLayout );

    connect( mainWindow, SIGNAL(GameObjectSelected(std::list< ae3d::GameObject* >)),
             this, SLOT(GameObjectSelected(std::list< ae3d::GameObject* >)) );
    connect( ortho, &QTableWidget::itemChanged, [&](QTableWidgetItem* /*item*/) { ApplyFieldsIntoSelectedCamera(); });
    connect( persp, &QTableWidget::itemChanged, [&](QTableWidgetItem* /*item*/) { ApplyFieldsIntoSelectedCamera(); });
    connect( clearFlagsBox, SIGNAL(currentIndexChanged(int)), this, SLOT(ClearFlagsChanged(int)) );
    connect( projectionBox, SIGNAL(currentIndexChanged(int)), this, SLOT(ProjectionChanged() ));
    connect( clearColorTable, &QTableWidget::itemChanged, [&](QTableWidgetItem* /*item*/) { ApplyFieldsIntoSelectedCamera(); });
    connect( removeButton, SIGNAL(clicked(bool)), mainWindow, SLOT(CommandRemoveCameraComponent()));
    connect( orderInput, &QLineEdit::textChanged, [&](const QString &/*text*/) { ApplyFieldsIntoSelectedCamera(); });
}

void CameraInspector::ProjectionChanged()
{
    ApplyFieldsIntoSelectedCamera();
}

void CameraInspector::ClearFlagsChanged( int )
{
    ApplyFieldsIntoSelectedCamera();
}

void CameraInspector::ApplySelectedCameraIntoFields( const ae3d::CameraComponent& camera )
{
    disconnect( ortho, &QTableWidget::itemChanged, ortho, nullptr );
    disconnect( persp, &QTableWidget::itemChanged, persp, nullptr );

    projectionBox->setCurrentIndex( camera.GetProjectionType() == ae3d::CameraComponent::ProjectionType::Orthographic ? 0 : 1 );

    if (camera.GetClearFlag() == ae3d::CameraComponent::ClearFlag::DepthAndColor)
    {
        clearFlagsBox->setCurrentIndex( 0 );
    }
    else if (camera.GetClearFlag() == ae3d::CameraComponent::ClearFlag::DontClear)
    {
        clearFlagsBox->setCurrentIndex( 2 );
    }
    else if (camera.GetClearFlag() == ae3d::CameraComponent::ClearFlag::Depth)
    {
        clearFlagsBox->setCurrentIndex( 1 );
    }

    persp->item( 0, 0 )->setText( FloatToQString( camera.GetFovDegrees() ) );
    persp->item( 0, 1 )->setText( FloatToQString( camera.GetAspect() ) );
    persp->item( 0, 2 )->setText( FloatToQString( camera.GetNear() ) );
    persp->item( 0, 3 )->setText( FloatToQString( camera.GetFar() ) );

    ortho->item( 0, 0 )->setText( FloatToQString( camera.GetRight() ) );
    ortho->item( 0, 1 )->setText( FloatToQString( camera.GetTop() ) );
    ortho->item( 0, 2 )->setText( FloatToQString( camera.GetNear() ) );
    ortho->item( 0, 3 )->setText( FloatToQString( camera.GetFar() ) );

    clearColorTable->item( 0, 0 )->setText( FloatToQString( camera.GetClearColor().x ) );
    clearColorTable->item( 0, 1 )->setText( FloatToQString( camera.GetClearColor().y ) );
    clearColorTable->item( 0, 2 )->setText( FloatToQString( camera.GetClearColor().z ) );

    orderInput->setText( IntToQString( (int)camera.GetRenderOrder() ) );

    connect( ortho, &QTableWidget::itemChanged, [&](QTableWidgetItem* /*item*/) { ApplyFieldsIntoSelectedCamera(); });
    connect( persp, &QTableWidget::itemChanged, [&](QTableWidgetItem* /*item*/) { ApplyFieldsIntoSelectedCamera(); });
}

void CameraInspector::ApplyFieldsIntoSelectedCamera()
{
    float width, height, fov, aspect, orthoNear, orthoFar, perspNear, perspFar;

    try
    {
        fov = std::stof( persp->item( 0, 0 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        fov = 45;
        persp->item( 0, 0 )->setText( "45" );
    }

    try
    {
        aspect = std::stof( persp->item( 0, 1 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        aspect = 1;
        persp->item( 0, 1 )->setText( "1" );
    }

    try
    {
        perspNear = std::stof( persp->item( 0, 2 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        perspNear = 1;
        persp->item( 0, 2 )->setText( "1" );
    }

    try
    {
        perspFar = std::stof( persp->item( 0, 3 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        perspFar = 1;
        persp->item( 0, 3 )->setText( "1" );
    }

    try
    {
        width = std::stof( ortho->item( 0, 0 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        width = 1;
        ortho->item( 0, 0 )->setText( "1" );
    }

    try
    {
        height = std::stof( ortho->item( 0, 1 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        height = 1;
        ortho->item( 0, 1 )->setText( "1" );
    }

    try
    {
        orthoNear = std::stof( ortho->item( 0, 2 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        orthoNear = 1;
        ortho->item( 0, 2 )->setText( "1" );
    }

    try
    {
        orthoFar = std::stof( ortho->item( 0, 3 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        orthoFar = 1;
        ortho->item( 0, 3 )->setText( "1" );
    }

    const ae3d::Vec4 orthoParams { width, height, orthoNear, orthoFar };
    const ae3d::Vec4 perspParams { fov, aspect, perspNear, perspFar };

    // TODO: init these from fields
    ae3d::Vec3 clearColor;

    try
    {
        clearColor.x = std::stof( clearColorTable->item( 0, 0 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        clearColor.x = 0;
        clearColorTable->item( 0, 0 )->setText( "0" );
    }

    try
    {
        clearColor.y = std::stof( clearColorTable->item( 0, 1 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        clearColor.y = 0;
        clearColorTable->item( 0, 1 )->setText( "0" );
    }

    try
    {
        clearColor.z = std::stof( clearColorTable->item( 0, 2 )->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {
        clearColor.z = 0;
        clearColorTable->item( 0, 2 )->setText( "0" );
    }

    int renderOrder = 0;

    try
    {
        renderOrder = std::stof( orderInput->text().toUtf8().constData() );
    }
    catch (std::invalid_argument&)
    {

    }

    ae3d::CameraComponent::ProjectionType projectionType = projectionBox->currentIndex() == 1 ?
                ae3d::CameraComponent::ProjectionType::Perspective : ae3d::CameraComponent::ProjectionType::Orthographic;

    CameraComponent::ClearFlag clearFlag = CameraComponent::ClearFlag::DontClear;

    if (clearFlagsBox->currentIndex() == 0)
    {
        clearFlag = CameraComponent::ClearFlag::DepthAndColor;
    }
    else if (clearFlagsBox->currentIndex() == 1)
    {
        clearFlag = CameraComponent::ClearFlag::Depth;
    }

    emit CameraModified( clearFlag, projectionType, orthoParams, perspParams, clearColor, renderOrder );
}

void CameraInspector::GameObjectSelected( std::list< ae3d::GameObject* > gameObjects )
{
    if (gameObjects.empty() || !gameObjects.front()->GetComponent< ae3d::CameraComponent >())
    {
        return;
    }

    disconnect( ortho, &QTableWidget::itemChanged, ortho, nullptr );
    disconnect( persp, &QTableWidget::itemChanged, persp, nullptr );
    ApplySelectedCameraIntoFields( *gameObjects.front()->GetComponent< ae3d::CameraComponent >() );
    connect( ortho, &QTableWidget::itemChanged, [&](QTableWidgetItem*) { ApplyFieldsIntoSelectedCamera(); });
    connect( persp, &QTableWidget::itemChanged, [&](QTableWidgetItem*) { ApplyFieldsIntoSelectedCamera(); });
}
